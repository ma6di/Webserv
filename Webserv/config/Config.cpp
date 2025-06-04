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

const std::string* Config::getErrorPage(int code) const {
    std::map<int, std::string>::const_iterator it = error_pages.find(code);
    if (it != error_pages.end())
        return &it->second;
    return NULL;
}

// Utility function to strip trailing semicolon
static std::string stripSemicolon(const std::string& token) {
    if (!token.empty() && token[token.size() - 1] == ';')
        return token.substr(0, token.size() - 1);
    return token;
}

int Config::parseListenDirective(const std::string& token) {
    std::string portStr = stripSemicolon(token);

    // ✅ Check digits only
    if (portStr.empty() || portStr.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid listen port: not a number");

    int parsedPort = std::atoi(portStr.c_str());

    // ✅ Check valid port range
    if (parsedPort <= 0 || parsedPort > 65535)
        throw std::runtime_error("Invalid listen port: must be between 1 and 65535");

    return parsedPort;
}

bool Config::pathExists(const std::string& path) {
    struct stat s;
    return stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode);
}

// Main config parser
void Config::parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + filename);

    std::string line;
	// flag for tracking if you're parsing inside a location block
    bool insideLocation = false;
	// temporary holder for values inside a location block
    LocationConfig currentLocation;

    while (std::getline(file, line)) {
        // Remove leading spaces and Skips empty lines and comments (#).
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line[0] == '#')
            continue;
		//Tokenizes the line.
        std::istringstream iss(line);
        std::string keyword;
		//Extracts the first word (keyword), like listen, root, location, etc.
        iss >> keyword;

		//Parses listen 8080;
		//Converts port string to int using atoi
		if (keyword == "listen") {
			std::string token;
			iss >> token;
			port = parseListenDirective(token);
		}
		//Parses root only if it's outside of a location block
		//Saves it as the global server root
		else if (keyword == "root" && !insideLocation) {
			std::string r;
			iss >> r;
			r = stripSemicolon(r);
			if (!pathExists(r))
			{
				std::cout << "[DEBUG] root value: [" << root << "]\n";
				throw std::runtime_error("Invalid root path: " + r);
			}
			root = r;
		}
		//Parses directives like error_page 404 /404.html;
		//Maps status code 404 to a file path /404.html
        else if (keyword == "error_page") {
            std::string codeStr, path;
            iss >> codeStr >> path;
            int code = std::atoi(stripSemicolon(codeStr).c_str());
            error_pages[code] = stripSemicolon(path);
        }
		//Detects start of a location block.
		//Resets the currentLocation object.
		//Records the path (e.g., /cgi-bin, /upload)
		//Sets insideLocation = true so further lines go into this object
        else if (keyword == "location") {
            std::string location_path;
            iss >> location_path;

            currentLocation = LocationConfig(); // Reset
            currentLocation.path = stripSemicolon(location_path);
            insideLocation = true;
        }
		//Detects end of location block.
		//Pushes the filled LocationConfig into the locations vector.
        else if (keyword == "}") {
            if (insideLocation) {
				// ✅ Apply defaults if not explicitly set
				if (currentLocation.index.empty())
				{
					std::cout << "⚠️  No index set for " << currentLocation.path << " → using default: index.html\n";
					currentLocation.index = "index.html";
				}
				if (currentLocation.allowed_methods.empty())
					currentLocation.allowed_methods.push_back("GET");
                locations.push_back(currentLocation);
                insideLocation = false;
				
            }
        }
        else if (insideLocation) {
			if (keyword == "root") {
				std::string r;
				iss >> r;
				r = stripSemicolon(r);
				if (!pathExists(r))
					throw std::runtime_error("Invalid location root path: " + r);
				currentLocation.root = r;
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
