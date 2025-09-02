#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "LocationConfig.hpp"
#include "../logger/Logger.hpp"
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <set>
#include <fstream>
#include <limits.h>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

class Config {
public:

	Config();
	//explicit keyword before a constructor prevents the compiler from using that constructor 
	//for implicit conversions and copy-initialization.
    explicit Config(const std::string& filename);

    std::vector<int> ports;
    const std::vector<int>& getPorts() const;
    std::vector<std::string> hosts;
    const std::vector<std::string>& getHosts() const;
    const std::string& getRoot() const;
    const std::vector<LocationConfig>& getLocations() const;
    const std::map<int, std::string>& getErrorPages() const;
	const std::string* getErrorPage(int code) const;
    size_t getMaxBodySize() const;

    //Helper Validating functions
	int parseListenDirective(const std::string& token);
	bool pathExists(const std::string& path);
    void parseServerBlock(std::ifstream& file);

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

};

std::vector<Config> parseConfigFile(const std::string& filename);

#endif
