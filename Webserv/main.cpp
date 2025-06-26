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
Config g_config("default.conf");

// Global flag to control server running state (used for graceful shutdown)
volatile sig_atomic_t g_running = 1;

// Global pointer for cleanup in signal handler
WebServer* g_server = NULL;

/**
 * SIGINT/SIGTERM handler: Sets running flag to 0 for graceful shutdown
 */
void sigint_handler(int) {
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

        Config g_config(config_file.c_str());

        struct sigaction sa_int;
        sa_int.sa_handler = sigint_handler;
        sigemptyset(&sa_int.sa_mask);
        sa_int.sa_flags = 0;
        sigaction(SIGINT, &sa_int, NULL);
        sigaction(SIGTERM, &sa_int, NULL);

        WebServer server(g_config.getPorts());
        g_server = &server;

        while (g_running) {
            Logger::log(LOG_DEBUG, "main", "before run one iteration");
            server.run();
            // Optionally reap any non-CGI children here if needed:
            // while (waitpid(-1, NULL, WNOHANG) > 0) {}
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
}
