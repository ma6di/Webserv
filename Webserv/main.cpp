#include "WebServer.hpp"


int main() {
    try {
        WebServer server(8080);
        server.run(); 
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }    
    return (0);
}