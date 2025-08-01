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


class Config {
public:

	Config(const std::string& filename);

    std::vector<int> ports;
    const std::vector<int>& getPorts() const;
    const std::string& getRoot() const;
    const std::vector<LocationConfig>& getLocations() const;
    const std::map<int, std::string>& getErrorPages() const;
	const std::string* getErrorPage(int code) const;
    size_t getMaxBodySize() const;

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

    int port;                                 
    std::string root;                         
    std::vector<LocationConfig> locations;    
    std::map<int, std::string> error_pages;   
	size_t max_body_size;

    void parseConfigFile(const std::string& filename);
};

#endif
