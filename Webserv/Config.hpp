#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationConfig {
    std::string path;
    std::string root;
    std::vector<std::string> allowed_methods;
    std::string index;
    std::string cgi_extension;

};

class Config {
public:
    Config(const std::string& filename);
    const std::string& getRoot() const;
    const std::vector<LocationConfig>& getLocations() const;

private:
    std::string root;
    std::vector<LocationConfig> locations;

    void parseConfigFile(const std::string& filename);
};

#endif

