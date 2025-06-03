#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "LocationConfig.hpp"
#include <string>
#include <vector>
#include <map>

// The Config class represents a full server block.
// It is responsible for loading a config file and storing all relevant settings.
class Config {
public:
    // Constructor takes the path to a config file and immediately parses it.
    Config(const std::string& filename);

    // Accessor methods for the parsed configuration.
    int getPort() const;
    const std::string& getRoot() const;
    const std::vector<LocationConfig>& getLocations() const;
    const std::map<int, std::string>& getErrorPages() const;

private:
    int port;                                 // Port the server will listen on
    std::string root;                         // Global root directory for the server
    std::vector<LocationConfig> locations;    // List of all location blocks (e.g. "/cgi-bin", "/upload")
    std::map<int, std::string> error_pages;   // Map of error codes to file paths (e.g., 404 → /404.html)

    // Internal method that performs the actual parsing logic
    void parseConfigFile(const std::string& filename);
};

#endif
