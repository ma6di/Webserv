#include "WebServer.hpp"
#include "Config.hpp"
#include "CGIHandler.hpp"
#include "Request.hpp"
#include "logger/Logger.hpp" // Add this include
#include <iostream>
#include <map>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <ctime>
#include <sstream>
#include <algorithm>

struct ClientState {
    std::string buffer;
    bool has_pending_write;
    time_t last_active;
    // maybe more...
};

volatile bool g_running = true;
std::vector<WebServer *> g_servers;

static void sigint_handler(int /*signum*/)
{
    g_running = false;
    // Shut down all servers immediately
    for (size_t i = 0; i < g_servers.size(); ++i)
    {
        g_servers[i]->shutdown();
    }
}

/**
 * Log configuration details for debugging purposes
 */
static void logConfigurationDetails(const std::vector<Config> &configs)
{
    // Iterate over all server configurations
    for (size_t ci = 0; ci < configs.size(); ++ci)
    {
        const Config &cfg = configs[ci];
        // Get all location blocks for this server
        const std::vector<LocationConfig> &locs = cfg.getLocations();
        for (size_t li = 0; li < locs.size(); ++li)
        {
            const LocationConfig &L = locs[li];
            // Build a comma-separated list of allowed HTTP methods for this location
            std::ostringstream methods;
            for (size_t mi = 0; mi < L.allowed_methods.size(); ++mi)
            {
                if (mi)
                    methods << ","; // Add comma between methods
                methods << L.allowed_methods[mi]; // Add method name
            }
            // Log the configuration details for this location:
            // - server index
            // - location index
            // - path
            // - allowed methods
            // - upload directory
            Logger::log(LOG_INFO,
                        "ConfigDump",
                        "server[" + to_str(ci) + "].loc[" + to_str(li) + "] "
                        "path='" + L.path + "' "
                        "methods=[" + methods.str() + "] "
                        "upload_dir='" + L.upload_dir + "'");
        }
    }
}

/**
 * Setup signal handlers for graceful shutdown
 */
static void setupSignalHandlers()
{
    // Create a sigaction struct to specify signal handling behavior
    struct sigaction sa;
    // Set the handler function for SIGINT and SIGTERM (Ctrl+C or kill)
    sa.sa_handler = sigint_handler;
    // Clear the signal mask (no signals are blocked during handler execution)
    sigemptyset(&sa.sa_mask);
    // No special flags
    sa.sa_flags = 0;
    // Register the handler for SIGINT (Ctrl+C)
    sigaction(SIGINT, &sa, 0);
    // Register the handler for SIGTERM (kill)
    sigaction(SIGTERM, &sa, 0);
}

/**
 * Create WebServer instances for each configuration
 */
static void createServers(const std::vector<Config> &configs)
{
    for (size_t i = 0; i < configs.size(); ++i)
    {
        WebServer *srv = new WebServer(configs[i]);
        g_servers.push_back(srv);
    }
}

/**
 * Build poll file descriptors for all servers
 */
static void buildPollFds(std::vector<struct pollfd> &fds)
{
    fds.clear();
    
    for (size_t si = 0; si < g_servers.size(); ++si)
    {
        WebServer *srv = g_servers[si];

        // listening sockets (only care about new connections)
        const std::vector<int> &ls = srv->getListeningSockets();
        for (size_t j = 0; j < ls.size(); ++j)
        {
            struct pollfd pfd;
            pfd.fd = ls[j];
            pfd.events = POLLIN;
            pfd.revents = 0;
            fds.push_back(pfd);
        }

        // client sockets (watch for read *and* conditional write)
        std::vector<int> cs = srv->getClientSockets();
        for (size_t j = 0; j < cs.size(); ++j)
        {
            int fd = cs[j];
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN | (srv->hasPendingWrite(fd) ? POLLOUT : 0);
            pfd.revents = 0;
            fds.push_back(pfd);
        }
    }
}

/**
 * Handle incoming connections and data
 */
static void handlePollIn(int fd)
{
    bool handled = false;

    // a) New connection?
    for (size_t si = 0; si < g_servers.size(); ++si)
    {
        WebServer *srv = g_servers[si];
        const std::vector<int> &ls = srv->getListeningSockets();
        for (size_t j = 0; j < ls.size(); ++j)
        {
            if (ls[j] == fd)
            {
                srv->handleNewConnectionOn(fd);
                handled = true;
                break;
            }
        }
        if (handled)
            break;
    }

    // b) Existing client data?
    if (!handled)
    {
        for (size_t si = 0; si < g_servers.size(); ++si)
        {
            WebServer *srv = g_servers[si];
            std::vector<int> cs = srv->getClientSockets();
            for (size_t j = 0; j < cs.size(); ++j)
            {
                if (cs[j] == fd)
                {
                    srv->handleClientDataOn(fd);
                    break;
                }
            }
        }
    }
}

static void handlePollOut(int fd)
{
    // Find which server owns this fd and flush its buffer
    for (size_t si = 0; si < g_servers.size(); ++si)
    {
        WebServer *srv = g_servers[si];
        // If this fd belongs to srv, it will handle it
        std::vector<int> cs = srv->getClientSockets();
        if (std::find(cs.begin(), cs.end(), fd) != cs.end())
        {
            srv->flushPendingWrites(fd);
            break;
        }
    }
}

/**
 * Handle poll events for all file descriptors
 */
static void handlePollEvents(const std::vector<struct pollfd> &fds)
{
    for (size_t pi = 0; pi < fds.size(); ++pi)
    {
        const struct pollfd &p = fds[pi];
        // 1) Incoming connection or data?
        if (p.revents & POLLIN)
        {
            handlePollIn(p.fd);
        }

        // 2) Ready to write?
        if (p.revents & POLLOUT)
        {
            handlePollOut(p.fd);
        }
    }
}

/**
 * Check for client timeouts and close idle connections
 */
static void checkClientTimeouts()
{
    time_t now = time(NULL);
    int client_timeout = 10; // 10 seconds timeout for testing

    for (size_t si = 0; si < g_servers.size(); ++si)
    {
        WebServer *srv = g_servers[si];
        std::vector<int> cs = srv->getClientSockets();
        
        // Check each client for timeout (iterate backwards to safely remove)
        for (int j = static_cast<int>(cs.size()) - 1; j >= 0; --j)
        {
            int fd = cs[j];
            time_t last_active = srv->getClientLastActive(fd);
            if (last_active > 0 && (now - last_active) > client_timeout)
            {
                Logger::log(LOG_INFO, "main", 
                          "Client fd=" + to_str(fd) + " timed out after " + 
                          to_str(now - last_active) + " seconds");
                srv->send_error_response(fd, 408, "Request Timeout", si);
                srv->flushPendingWrites(fd);
				srv->closeClient(fd);
            }
        }
    }
}

/**
 * Main server loop that handles all polling and events
 */
static void runServerLoop()
{
    while (g_running)
    {
        std::vector<struct pollfd> fds;
        buildPollFds(fds);

        int ret = poll(&fds[0], fds.size(), 1000); // wait 1 second max

        if (ret < 0 && errno == EINTR)
        {
            break;
        }

        // Handle ready file descriptors
        handlePollEvents(fds);

        // Check for client timeouts
        checkClientTimeouts();
    }
}

/**
 * Cleanup all servers and free memory
 */
static void cleanupServers()
{
    for (size_t si = 0; si < g_servers.size(); ++si)
    {
        delete g_servers[si];
    }
    g_servers.clear();
}

int main(int argc, char **argv)
{
    try
    {
        // 1) Determine config file path
        std::string config_file = "default.conf";
        if (argc == 2)
        {
            config_file = argv[1];
        }

        if (argc > 2)
        {
            std::cout << "Please provide a single config file path as the first argument." << std::endl;
            std::cout << "If you do not provide any argument, default config file will be used." << std::endl;
            return 1;
        }
        // 2) Parse all server blocks
        std::vector<Config> configs = parseConfigFile(config_file);

        // 3) Log configuration details
        logConfigurationDetails(configs);

        // 4) Install signal handlers for graceful shutdown
        setupSignalHandlers();

        // 5) Instantiate one WebServer per Config
        createServers(configs);

        // 6) Unified non-blocking loop
        runServerLoop();

        // 7) Cleanup
        cleanupServers();
    }
    catch (const std::exception &e)
    {
        Logger::log(LOG_ERROR, "main", std::string("Fatal error: ") + e.what());
        cleanupServers();
        return 1;
    }

    return 0;
}
