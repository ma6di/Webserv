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
	std::string redirect_url; 
	int redirect_code; 
	bool autoindex;  

    LocationConfig() : autoindex(false) {} 
};

#endif
