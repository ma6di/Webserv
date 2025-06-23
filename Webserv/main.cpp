#include "WebServer.hpp"
#include "config/Config.hpp"
#include "cgi/CGIHandler.hpp"
#include "http/Request.hpp"
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
        std::cout << "WebServer shut down by signal." << std::endl;
    }
}

int main(int argc, char** argv) {
    try {
        // Parse command-line arguments for port and config file
        int port = 8080;
        std::string config_file = "default.conf";
        if (argc > 1) port = atoi(argv[1]);
        if (argc > 2) config_file = argv[2];

        // Update global config with the chosen config file
        Config g_config(config_file.c_str());

        // Set up signal handlers for graceful shutdown
        struct sigaction sa_int;
        sa_int.sa_handler = sigint_handler;
        sigemptyset(&sa_int.sa_mask);
        sa_int.sa_flags = 0;
        sigaction(SIGINT, &sa_int, NULL);
        sigaction(SIGTERM, &sa_int, NULL);

        // Start the WebServer on the specified port
        WebServer server(g_config.getPorts());
        g_server = &server;
        std::cout << "WebServer starting on port " << port << " with config: " << config_file << std::endl;

        // Main server loop: runs until interrupted
        while (g_running) {
            std::cout << "[DEBUG] before run one iteration " << std::endl;
            server.run(); // Handles one poll/select cycle

            // Optionally reap any non-CGI children here if needed:
            // while (waitpid(-1, NULL, WNOHANG) > 0) {}
        }

        // Shutdown: clean up sockets and resources
        server.shutdown();
        std::cout << "WebServer shut down gracefully." << std::endl;
    } catch (const std::bad_alloc& e) {
        if (g_server) g_server->shutdown();
        std::cerr << "Fatal error: Out of memory (" << e.what() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        if (g_server) g_server->shutdown();
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
