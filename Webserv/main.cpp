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

// Global configuration object, initialized with config file
/*Config g_config("default.conf");

// Global flag to control server running state (used for graceful shutdown)
volatile sig_atomic_t g_running = 1;

// Global pointer for cleanup in signal handler
WebServer* g_server = NULL;

*/
/**
 * SIGINT/SIGTERM handler: Sets running flag to 0 for graceful shutdown
 */
/*void sigint_handler(int) {
    g_running = 0;
    if (g_server) {
        g_server->shutdown();
        Logger::log(LOG_INFO, "main", "WebServer shut down by signal.");
    }
}

int main(int argc, char** argv) {
    try {
        std::string config_file = "default.conf";
        if (argc > 2) config_file = argv[2];

        //Config g_config(config_file.c_str());
        std::vector<Config> configs = parseConfigFile(config_file);

        struct sigaction sa_int;
        sa_int.sa_handler = sigint_handler;
        sigemptyset(&sa_int.sa_mask);
        sa_int.sa_flags = 0;
        sigaction(SIGINT, &sa_int, NULL);
        sigaction(SIGTERM, &sa_int, NULL);

        //WebServer server(g_config.getPorts());
        //g_server = &server;

        while (g_running) {
            for (auto* srv : servers) {
                srv->run_one_iteration();
            }
        }

        server.shutdown();
        Logger::log(LOG_INFO, "main", "WebServer shut down gracefully.");
    } catch (const std::bad_alloc& e) {
        if (g_server) g_server->shutdown();
        Logger::log(LOG_ERROR, "main", std::string("Fatal error: Out of memory (") + e.what() + ")");
        return 1;
    } catch (const std::exception& e) {
        if (g_server) g_server->shutdown();
        Logger::log(LOG_ERROR, "main", std::string("Server error: ") + e.what());
        return 1;
    }
    return 0;
}*/

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

int main(int argc, char **argv)
{
    try
    {
        // 1) Determine config file path
        std::string config_file = "default.conf";
        if (argc > 1)
        {
            config_file = argv[1];
        }

        if (argc > 1)
        {
            std::cout << "Please provide a single config file path as the first argument." << std::endl;
            std::cout << "If you do not provide any argument, default config file will be used." << std::endl;
            return 1;
        }
        // 2) Parse all server blocks
        std::vector<Config> configs = parseConfigFile(config_file);

        for (size_t ci = 0; ci < configs.size(); ++ci)
        {
            const Config &cfg = configs[ci];
            const std::vector<LocationConfig> &locs = cfg.getLocations();
            for (size_t li = 0; li < locs.size(); ++li)
            {
                const LocationConfig &L = locs[li];
                // build a comma-separated list of methods
                std::ostringstream methods;
                for (size_t mi = 0; mi < L.allowed_methods.size(); ++mi)
                {
                    if (mi)
                        methods << ",";
                    methods << L.allowed_methods[mi];
                }
                Logger::log(LOG_INFO,
                            "ConfigDump",
                            "server[" + to_str(ci) + "].loc[" + to_str(li) + "] "
                                                                             "path='" +
                                L.path +
                                "' methods=[" + methods.str() + "] "
                                                                "upload_dir='" +
                                L.upload_dir + "'");
            }
        }
        // 3) Install signal handlers for graceful shutdown
        struct sigaction sa;
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, 0);
        sigaction(SIGTERM, &sa, 0);

        // 4) Instantiate one WebServer per Config
        for (size_t i = 0; i < configs.size(); ++i)
        {
            WebServer *srv = new WebServer(configs[i]);
            g_servers.push_back(srv);
        }

        // 5) Unified non-blocking loop
        while (g_running)
        {
            std::vector<struct pollfd> fds;

            // build the pollfd list
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

            int ret = poll(&fds[0], fds.size(), 1000); // wait 1 second max

            if (ret < 0 && errno == EINTR)
            {
                break;
            }

            // handle ready fds
            for (size_t pi = 0; pi < fds.size(); ++pi)
            {
                struct pollfd &p = fds[pi];

                // 1) Incoming connection or data?
                if (p.revents & POLLIN)
                {
                    bool handled = false;

                    // a) New connection?
                    for (size_t si = 0; si < g_servers.size(); ++si)
                    {
                        WebServer *srv = g_servers[si];
                        const std::vector<int> &ls = srv->getListeningSockets();
                        for (size_t j = 0; j < ls.size(); ++j)
                        {
                            if (ls[j] == p.fd)
                            {
                                srv->handleNewConnectionOn(p.fd);
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
                                if (cs[j] == p.fd)
                                {
                                    srv->handleClientDataOn(p.fd);
                                    break;
                                }
                            }
                        }
                    }
                }

                // 2) Ready to write?
                if (p.revents & POLLOUT)
                {
                    // Find which server owns this fd and flush its buffer
                    for (size_t si = 0; si < g_servers.size(); ++si)
                    {
                        WebServer *srv = g_servers[si];
                        // If this fd belongs to srv, it will handle it
                        std::vector<int> cs = srv->getClientSockets();
                        if (std::find(cs.begin(), cs.end(), p.fd) != cs.end())
                        {
                            srv->flushPendingWrites(p.fd);
                            break;
                        }
                    }
                }
            }

            // 3) Check for client timeouts (sweep idle clients)
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
                        srv->send_request_timeout_response(fd, si);
                        srv->closeClient(fd);
                    }
                }
            }
        }

        for (size_t si = 0; si < g_servers.size(); ++si)
        {
            delete g_servers[si];
        }
        g_servers.clear();
    }
    catch (const std::exception &e)
    {
        Logger::log(LOG_ERROR, "main", std::string("Fatal error: ") + e.what());

        for (size_t si = 0; si < g_servers.size(); ++si)
        {
            delete g_servers[si];
        }
        g_servers.clear();
        return 1;
    }

    return 0;
}
