#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

struct LocationConfig {
    std::string path;
    std::string root;
    std::vector<std::string> allowed_methods;
    std::string index;
    std::string cgi_extension;
    std::string upload_dir;
    bool autoindex;  // ✅ Remove "= false" (not allowed in C++98)

    LocationConfig() : autoindex(false) {} // ✅ C++98-compatible initialization
};

#endif
