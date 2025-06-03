#include "WebServer.hpp"
#include "config/Config.hpp"
#include "cgi/CGIHandler.hpp"
#include "http/Request.hpp"
#include <iostream>
#include <map>

Config g_config("default.conf");

int main() {
    try {
        WebServer server(8080);
        server.run(); 
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }    
    return (0);
}
