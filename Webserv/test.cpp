#include "config/Config.hpp"
#include "cgi/CGIHandler.hpp"
#include "http/Request.hpp"
#include <iostream>
#include <map>

Config g_config("default.conf");

void test_config(const Config& config) {
    std::cout << "Port: " << config.getPort() << "\n";
    std::cout << "Root: " << config.getRoot() << "\n";

    const std::vector<LocationConfig>& locs = config.getLocations();
    for (size_t i = 0; i < locs.size(); ++i) {
        const LocationConfig& loc = locs[i];
        std::cout << "Location: " << loc.path << "\n";
        if (!loc.root.empty())
            std::cout << "  Root: " << loc.root << "\n";
        if (!loc.index.empty())
            std::cout << "  Index: " << loc.index << "\n";
        if (!loc.allowed_methods.empty()) {
            std::cout << "  Methods: ";
            for (size_t j = 0; j < loc.allowed_methods.size(); ++j)
                std::cout << loc.allowed_methods[j] << " ";
            std::cout << "\n";
        }
        if (!loc.cgi_extension.empty())
            std::cout << "  CGI Extension: " << loc.cgi_extension << "\n";
        if (!loc.upload_dir.empty())
            std::cout << "  Upload Dir: " << loc.upload_dir << "\n";
        std::cout << "  Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
    }
}

void test_cgi(const std::string& script_path) {
    std::map<std::string, std::string> env;
    env["REQUEST_METHOD"] = "GET";
    env["SCRIPT_NAME"] = "/cgi-bin/test.py";
    env["QUERY_STRING"] = "";

    CGIHandler handler(script_path, env);
    std::string result = handler.execute();

    std::cout << "\n---- CGI Output ----\n" << result << "\n";
}

int main() {
    try {
        Config config("default.conf");

        std::cout << "=== Testing Config ===\n";
        test_config(config);

        std::cout << "\n=== Testing CGI ===\n";
        test_cgi("./www/cgi-bin/test.py");

        // Optionally: simulate a real request for further integration
        std::string raw_http = "GET /cgi-bin/test.py HTTP/1.1\r\nHost: localhost\r\n\r\n";
        Request req(raw_http);
        std::cout << "\n=== Testing Request Parsing ===\n";
        std::cout << "Method: " << req.getMethod() << "\n";
        std::cout << "Path: " << req.getPath() << "\n";
        std::cout << "Version: " << req.getVersion() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
