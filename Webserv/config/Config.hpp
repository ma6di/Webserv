#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "LocationConfig.hpp"
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>

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
    int getPort() const;
	//global root, used as fallback if location doesn’t define one
    const std::string& getRoot() const;
	//list of all parsed location blocks
    const std::vector<LocationConfig>& getLocations() const;
	//map of HTTP error codes to custom error page paths
    const std::map<int, std::string>& getErrorPages() const;
	const std::string* getErrorPage(int code) const;

    //Helper Validating functions
	int parseListenDirective(const std::string& token);
	bool pathExists(const std::string& path);

private:
    int port;                                 // Port the server will listen on
    std::string root;                         // Global root directory for the server
    std::vector<LocationConfig> locations;    // List of all location blocks (e.g. "/cgi-bin", "/upload")
    std::map<int, std::string> error_pages;   // Map of error codes to file paths (e.g., 404 → /404.html)

    // Internal method that performs the actual parsing logic
    void parseConfigFile(const std::string& filename);
};

#endif
