#include "Config.hpp"

Config::Config() : port(0), root(""), max_body_size(1048576) {}

Config::Config(const std::string &filename) : max_body_size(1048576) { 
    parseConfigFile(filename);
}

const std::string &Config::getRoot() const { return root; }
const std::vector<LocationConfig> &Config::getLocations() const { return locations; }
const std::map<int, std::string> &Config::getErrorPages() const {
    return error_pages;
}

const std::string *Config::getErrorPage(int code) const {
    std::map<int, std::string>::const_iterator it = error_pages.find(code);
    if (it != error_pages.end())
        return &it->second;
    return NULL;
}

static std::string stripSemicolon(const std::string &token) {
    if (!token.empty() && token[token.size() - 1] == ';')
        return token.substr(0, token.size() - 1);
    return token;
}


int Config::parseListenDirective(const std::string &token) {
    std::string portStr = stripSemicolon(token);

    if (portStr.empty() || portStr.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid listen port: not a number");

    int parsedPort = std::atoi(portStr.c_str());

    if (parsedPort <= 0 || parsedPort > 65535)
        throw std::runtime_error("Invalid listen port: must be between 1 and 65535");

    return parsedPort;
}


bool Config::pathExists(const std::string &path) {
    struct stat s;
    return stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode);
}

static bool isValidIPv4(const std::string &s) {
    int dots = 0;
    int num = 0;
    int count = 0;

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.') {
            if (count == 0 || num > 255) return false;
            dots++; num = 0; count = 0;
        } else if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');
            count++;
            if (num > 255) return false;
        } else {
            return false;
        }
    }
    return (dots == 3 && num <= 255 && count > 0);
}

static bool checkHost(const std::string &host) {
    if (host == "localhost")
        return true;
    if (isValidIPv4(host))
        return true;
    return false;
}

void Config::handleListenDirective(std::istringstream &iss)
{
    std::string token;
    iss >> token;
    if (token.empty())
        throw std::runtime_error("listen: missing value");

    // Remove optional trailing ';'
    if (!token.empty() && token[token.size() - 1] == ';')
        token.erase(token.size() - 1);

// must be HOST:PORT
    std::string::size_type colon = token.find(':');
    if (colon == std::string::npos)
        throw std::runtime_error("listen: must be host:port (e.g., 127.0.0.1:8080)");

    std::string host = token.substr(0, colon);
    std::string portPart = token.substr(colon + 1);

    if (host.empty())
        throw std::runtime_error("listen: missing host before ':'");
    if (!checkHost(host))
        throw std::runtime_error("listen: invalid host '" + host + "' (must be IPv4 like 127.0.0.1 or 'localhost')");
    if (portPart.empty())
        throw std::runtime_error("listen: missing port after ':'");

    // validate port as before
    int parsed = parseListenDirective(portPart);

    ports.push_back(parsed);
    hosts.push_back(host);  // always present now

    Logger::log(
        LOG_DEBUG, "Config",
        std::string("listen: host=[") + host + "], port=" + to_str(parsed));

    // keep your existing "first port is primary" behavior
    if (port == 0)
        port = parsed;
    else if (port != parsed)
        Logger::log(LOG_DEBUG, "Config",
                    "Multiple listen ports detected, using first: " + to_str(port));

    if (parsed < 1 || parsed > 65535)
    {
        std::ostringstream oss;
        oss << "Invalid listen port: " << parsed;
        throw std::runtime_error(oss.str());
    }
}

void Config::handleRootDirective(std::istringstream &iss)
{
    std::string r;
    iss >> r;
    r = stripSemicolon(r);
    if (!pathExists(r))
    {
        Logger::log(LOG_DEBUG, "Config", "root value: [" + root + "]");
        throw std::runtime_error("Invalid root path: " + r);
    }
    root = r;
}

void Config::handleErrorPageDirective(std::istringstream &iss)
{
    std::string codeStr, path;
    iss >> codeStr >> path;
    int code = std::atoi(stripSemicolon(codeStr).c_str());
    error_pages[code] = stripSemicolon(path);
}

void Config::handleLocationStart(std::istringstream &iss, LocationConfig &currentLocation, bool &insideLocation)
{
    std::string location_path;
    iss >> location_path;
    currentLocation = LocationConfig();
    currentLocation.path = stripSemicolon(location_path);
    insideLocation = true;
}

void Config::handleClientMaxBodySizeDirective(std::istringstream &iss)
{
    std::string sizeStr;
    iss >> sizeStr;
    sizeStr = stripSemicolon(sizeStr);

    if (sizeStr.empty())
        throw std::runtime_error("client_max_body_size: missing value");

    // must be digits only
    for (size_t i = 0; i < sizeStr.size(); ++i) {
        if (sizeStr[i] < '0' || sizeStr[i] > '9') {
            throw std::runtime_error("client_max_body_size: must be a positive integer (digits only)");
        }
    }

    // convert to int
    long n = std::atol(sizeStr.c_str());

    if (n <= 0)
        throw std::runtime_error("client_max_body_size: must be >= 1");

    if (n > INT_MAX)  // too large for safety
        throw std::runtime_error("client_max_body_size: value too large");

    max_body_size = static_cast<size_t>(n);
}

void Config::handleLocationEnd(LocationConfig &currentLocation, bool &insideLocation)
{
    if (insideLocation)
    {
        if (currentLocation.index.empty())
        {
            Logger::log(LOG_INFO, "Config", "No index set for " + currentLocation.path + " → using default: index.html");
            currentLocation.index = "index.html";
        }
        if (currentLocation.allowed_methods.empty())
            currentLocation.allowed_methods.push_back("GET");
        locations.push_back(currentLocation);
        insideLocation = false;
    }
}

void Config::handleLocationDirective(const std::string &keyword, std::istringstream &iss, LocationConfig &currentLocation)
{
    if (keyword == "root")
    {
        std::string r;
        iss >> r;
        r = stripSemicolon(r);
        if (!pathExists(r))
            throw std::runtime_error("Invalid location root path: " + r);
        currentLocation.root = r;
    }
    else if (keyword == "index")
    {
        std::string indexFile;
        iss >> indexFile;
        currentLocation.index = stripSemicolon(indexFile);
    }
    else if (keyword == "methods")
    {
        std::string method;
        while (iss >> method)
            currentLocation.allowed_methods.push_back(stripSemicolon(method));
    }
    else if (keyword == "cgi_extension")
    {
        std::string ext;
        iss >> ext;
        currentLocation.cgi_extension = stripSemicolon(ext);
    }
    else if (keyword == "upload_dir")
    {
        std::string dir;
        iss >> dir;
        currentLocation.upload_dir = stripSemicolon(dir);
    }
    else if (keyword == "autoindex")
    {
        std::string value;
        iss >> value;
        value = stripSemicolon(value);
        currentLocation.autoindex = (value == "on");
    }
    else if (keyword == "return")
    {
        std::string code_str, url;
        iss >> code_str >> url;
        currentLocation.redirect_code = std::atoi(code_str.c_str());
        currentLocation.redirect_url = stripSemicolon(url);
    }
}

size_t Config::getMaxBodySize() const {return max_body_size;}

const std::vector<int> &Config::getPorts() const {return ports;}

const std::vector<std::string>& Config::getHosts() const {return hosts;}

std::vector<Config> parseConfigFile(const std::string &filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw std::runtime_error("Could not open config file");

    std::vector<Config> servers;
    std::string line;
    while (std::getline(file, line))
    {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) 
            continue;
        std::string trimmed = line.substr(start);
        if (trimmed.empty() || trimmed[0] == '#')
            continue;
        if (trimmed.rfind("server", 0) == 0 && trimmed.find("{") != std::string::npos)
        {
            Config cfg;
            cfg.parseServerBlock(file);
            servers.push_back(cfg);
        }
    }
    return servers;
}

void Config::parseServerBlock(std::ifstream &file) {
    bool insideLocation = false;
    int braceDepth = 1;

    LocationConfig currentLocation;

    std::string line;
    while (std::getline(file, line))
    {

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "location")
        {
            handleLocationStart(iss, currentLocation, insideLocation);
            braceDepth++;
            continue;
        }

        if (insideLocation)
        {
            if (keyword == "{") {
                braceDepth++;
            }
            else if (keyword == "}") {
                handleLocationEnd(currentLocation, insideLocation);
                braceDepth--;
            }
            else
                handleLocationDirective(keyword, iss, currentLocation);
            continue;
        }

        if (keyword == "{") {
            braceDepth++;
        }
        else if (keyword == "}") {
            braceDepth--;
            if (braceDepth == 0) {
                break;
            }
        }
        else {
            if (keyword == "listen")
                handleListenDirective(iss);
            else if (keyword == "root")
                handleRootDirective(iss);
            else if (keyword == "error_page")
                handleErrorPageDirective(iss);
            else if (keyword == "client_max_body_size")
                handleClientMaxBodySizeDirective(iss);
        }
    }
    if (!ports.empty())
        port = ports.front();
}
