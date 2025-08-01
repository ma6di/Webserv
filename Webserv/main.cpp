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

// Global configuration object, initialized with config file
/*Config g_config("default.conf");

// Global flag to control server running state (used for graceful shutdown)
volatile sig_atomic_t g_running = 1;

// Global pointer for cleanup in signal handler
WebServer* g_server = NULL;

*//**
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

volatile bool g_running = true;
std::vector<WebServer*> g_servers;

static void sigint_handler(int /*signum*/) {
    g_running = false;
    // Shut down all servers immediately
    for (size_t i = 0; i < g_servers.size(); ++i) {
        g_servers[i]->shutdown();
    }
}

int main(int argc, char** argv) {
    try {
        // 1) Determine config file path
        std::string config_file = "default.conf";
        if (argc > 1) {
            config_file = argv[1];
        }

        // 2) Parse all server blocks
        std::vector<Config> configs = parseConfigFile(config_file);

        // 3) Install signal handlers for graceful shutdown
        struct sigaction sa;
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        // 4) Instantiate one WebServer per Config
        for (size_t i = 0; i < configs.size(); ++i) {
            WebServer* srv = new WebServer(configs[i]);
            g_servers.push_back(srv);
        }

        // 5) Unified non-blocking loop
        while (g_running) {
            for (size_t i = 0; i < g_servers.size(); ++i) {
                g_servers[i]->run_one_iteration();
            }
        }

        // 6) Cleanup
        for (size_t i = 0; i < g_servers.size(); ++i) {
            g_servers[i]->shutdown();
            delete g_servers[i];
        }
        g_servers.clear();

        Logger::log(LOG_INFO, "main", "All servers shut down gracefully.");
    }
    catch (const std::exception& e) {
        Logger::log(LOG_ERROR, "main", std::string("Fatal error: ") + e.what());
        return 1;
    }

    return 0;
}
