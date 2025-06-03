#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

// Constructor: read and parse the config file immediately
Config::Config(const std::string& filename) {
    parseConfigFile(filename);
}

// Simple getter methods
int Config::getPort() const { return port; }
const std::string& Config::getRoot() const { return root; }
const std::vector<LocationConfig>& Config::getLocations() const { return locations; }
const std::map<int, std::string>& Config::getErrorPages() const { return error_pages; }

// Utility function to strip trailing semicolon
static std::string stripSemicolon(const std::string& token) {
    if (!token.empty() && token[token.size() - 1] == ';')
        return token.substr(0, token.size() - 1);
    return token;
}

// Main config parser
void Config::parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filename);

    std::string line;
    bool insideLocation = false;
    LocationConfig currentLocation;

    while (std::getline(file, line)) {
        // Remove leading spaces
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "listen") {
            std::string portStr;
            iss >> portStr;
            port = std::atoi(stripSemicolon(portStr).c_str());
        }
        else if (keyword == "root" && !insideLocation) {
            std::string r;
            iss >> r;
            root = stripSemicolon(r);
        }
        else if (keyword == "error_page") {
            std::string codeStr, path;
            iss >> codeStr >> path;
            int code = std::atoi(stripSemicolon(codeStr).c_str());
            error_pages[code] = stripSemicolon(path);
        }
        else if (keyword == "location") {
            std::string location_path;
            iss >> location_path;

            currentLocation = LocationConfig(); // Reset
            currentLocation.path = stripSemicolon(location_path);
            insideLocation = true;
        }
        else if (keyword == "}") {
            if (insideLocation) {
                locations.push_back(currentLocation);
                insideLocation = false;
            }
        }
        else if (insideLocation) {
            if (keyword == "root") {
                std::string r;
                iss >> r;
                currentLocation.root = stripSemicolon(r);
            }
            else if (keyword == "index") {
                std::string indexFile;
                iss >> indexFile;
                currentLocation.index = stripSemicolon(indexFile);
            }
            else if (keyword == "methods") {
                std::string method;
                while (iss >> method)
                    currentLocation.allowed_methods.push_back(stripSemicolon(method));
            }
            else if (keyword == "cgi_extension") {
                std::string ext;
                iss >> ext;
                currentLocation.cgi_extension = stripSemicolon(ext);
            }
            else if (keyword == "upload_dir") {
                std::string dir;
                iss >> dir;
                currentLocation.upload_dir = stripSemicolon(dir);
            }
            else if (keyword == "autoindex") {
                std::string value;
                iss >> value;
                value = stripSemicolon(value);
                currentLocation.autoindex = (value == "on");
            }
        }
    }

    file.close();
}
