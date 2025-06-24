#include "Config.hpp"


// Constructor: read and parse the config file immediately
Config::Config(const std::string& filename) : max_body_size(1048576) { // 1MB default
    parseConfigFile(filename);
}

// Simple getter methods
//int Config::getPort() const { return port; }
const std::string& Config::getRoot() const { return root; }
const std::vector<LocationConfig>& Config::getLocations() const { return locations; }
const std::map<int, std::string>& Config::getErrorPages() const {
    return error_pages;
}

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
    bool insideLocation = false;
    LocationConfig currentLocation;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "listen") {
            handleListenDirective(iss);
        }
        else if (keyword == "root" && !insideLocation) {
            handleRootDirective(iss);
        }
        else if (keyword == "error_page") {
            handleErrorPageDirective(iss);
        }
        else if (keyword == "location") {
            handleLocationStart(iss, currentLocation, insideLocation);
        }
        else if (keyword == "client_max_body_size") {
            handleClientMaxBodySizeDirective(iss);
        }
        else if (keyword == "}") {
            handleLocationEnd(currentLocation, insideLocation);
        }
        else if (insideLocation) {
            handleLocationDirective(keyword, iss, currentLocation);
        }
    }

    file.close();
    std::cout << "Loaded config file: " << filename << std::endl;
}

// --- Helper functions ---

void Config::handleListenDirective(std::istringstream& iss) {
    std::string token;
    iss >> token;
    int parsed = parseListenDirective(token);
    ports.push_back(parsed);
    std::cout << "[DEBUG] listen port: " << parsed << std::endl;
    if (port == 0) {
        port = parsed; // Set the first parsed port as the default
    } else if (port != parsed) {
        std::cout << "[DEBUG] Multiple listen ports detected, using first: " << port << std::endl;
    }
    if (parsed < 1 || parsed > 65535) {
        std::ostringstream oss;
        oss << "Invalid listen port: " << parsed;
        throw std::runtime_error(oss.str());
    }
}

void Config::handleRootDirective(std::istringstream& iss) {
    std::string r;
    iss >> r;
    r = stripSemicolon(r);
    if (!pathExists(r)) {
        std::cout << "[DEBUG] root value: [" << root << "]\n";
        throw std::runtime_error("Invalid root path: " + r);
    }
    root = r;
}

void Config::handleErrorPageDirective(std::istringstream& iss) {
    std::string codeStr, path;
    iss >> codeStr >> path;
    int code = std::atoi(stripSemicolon(codeStr).c_str());
    error_pages[code] = stripSemicolon(path);
}

void Config::handleLocationStart(std::istringstream& iss, LocationConfig& currentLocation, bool& insideLocation) {
    std::string location_path;
    iss >> location_path;
    currentLocation = LocationConfig();
    currentLocation.path = stripSemicolon(location_path);
    insideLocation = true;
}

void Config::handleClientMaxBodySizeDirective(std::istringstream& iss) {
    std::string sizeStr;
    iss >> sizeStr;
    sizeStr = stripSemicolon(sizeStr);
    max_body_size = static_cast<size_t>(std::strtoull(sizeStr.c_str(), NULL, 10));
}

void Config::handleLocationEnd(LocationConfig& currentLocation, bool& insideLocation) {
    if (insideLocation) {
        if (currentLocation.index.empty()) {
            std::cout << "⚠️  No index set for " << currentLocation.path << " → using default: index.html\n";
            currentLocation.index = "index.html";
        }
        if (currentLocation.allowed_methods.empty())
            currentLocation.allowed_methods.push_back("GET");
        locations.push_back(currentLocation);
        insideLocation = false;
    }
}

void Config::handleLocationDirective(const std::string& keyword, std::istringstream& iss, LocationConfig& currentLocation) {
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
    else if (keyword == "return") {
    std::string code_str, url;
    iss >> code_str >> url;
    currentLocation.redirect_code = std::atoi(code_str.c_str());
    currentLocation.redirect_url = stripSemicolon(url);
    }
}

size_t Config::getMaxBodySize() const {
    return max_body_size; // Make sure you have a member variable for this
}

const std::vector<int>& Config::getPorts() const {
    return ports;
}
