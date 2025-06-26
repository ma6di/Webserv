#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "LocationConfig.hpp"
#include "../logger/Logger.hpp"
#include "WebServer.hpp"
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <set>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdlib>

// The Config class represents a full server block.
// It is responsible for loading a config file and storing all relevant settings.
class Config {
public:
    // Constructor takes the path to a config file and immediately parses it.
    //So when you write Config conf("default.conf"); it loads config on the spot
	Config(const std::string& filename);

    // Accessor methods for the parsed configuration.
	//All are marked const, meaning they don’t modify the object.

	//returns port from listen 8080;
    //updated to keep a list of ports
    std::vector<int> ports;
    const std::vector<int>& getPorts() const;
	//global root, used as fallback if location doesn’t define one
    const std::string& getRoot() const;
	//list of all parsed location blocks
    const std::vector<LocationConfig>& getLocations() const;
	//map of HTTP error codes to custom error page paths
    const std::map<int, std::string>& getErrorPages() const;
	const std::string* getErrorPage(int code) const;
	// Add this line to your public section
    size_t getMaxBodySize() const;

    //Helper Validating functions
	int parseListenDirective(const std::string& token);
	bool pathExists(const std::string& path);

private:
    void handleListenDirective(std::istringstream& iss);
    void handleRootDirective(std::istringstream& iss);
    void handleErrorPageDirective(std::istringstream& iss);
    void handleLocationStart(std::istringstream& iss, LocationConfig& currentLocation, bool& insideLocation);
    void handleClientMaxBodySizeDirective(std::istringstream& iss);
    void handleLocationEnd(LocationConfig& currentLocation, bool& insideLocation);
    void handleLocationDirective(const std::string& keyword, std::istringstream& iss, LocationConfig& currentLocation);

    int port;                                 // Port the server will listen on
    std::string root;                         // Global root directory for the server
    std::vector<LocationConfig> locations;    // List of all location blocks (e.g. "/cgi-bin", "/upload")
    std::map<int, std::string> error_pages;   // Map of error codes to file paths (e.g., 404 → /404.html)
	size_t max_body_size;
    // Internal method that performs the actual parsing logic
    void parseConfigFile(const std::string& filename);
};

#endif
