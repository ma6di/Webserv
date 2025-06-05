#include "WebServer.hpp"
#include "config/Config.hpp"
#include "cgi/CGIHandler.hpp"
#include "http/Request.hpp"
#include <iostream>
#include <map>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

Config g_config("default.conf");

void sigchld_handler(int) {
    // Reap all dead children
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main() {
	struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    try {
        WebServer server(8080);
        server.run(); 
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }    
    return (0);
}
