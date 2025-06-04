#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

//a struct to represent a location block from the configuration file
struct LocationConfig {
	//Holds the location path itself, like / or /cgi-bin or /upload.
    std::string path;
	//Root directory on disk to serve files from for this location.(/www/html or /www/uploads)
    std::string root;
	//List of allowed HTTP methods (GET, POST, etc.) for this location.
    std::vector<std::string> allowed_methods;
	//Default file to serve when a directory is requested (like index.html).
    std::string index;
	//If this location is configured for CGI, this holds the script extension (e.g., .py, .php).
	//Used to detect which files to treat as CGI.
    std::string cgi_extension;
	//Directory path where uploaded files should be saved (if upload is enabled).
	//Only used if the request is a POST and this field is defined.
    std::string upload_dir;
	//f set to true, and no index file is found, generate a listing of files in the directory.
    bool autoindex;  // ✅ Remove "= false" (not allowed in C++98)

    LocationConfig() : autoindex(false) {} // ✅ C++98-compatible initialization
};

#endif
