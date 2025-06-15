#ifndef UTILS_HPP
#define UTILS_HPP

#include "LocationConfig.hpp"
#include "Config.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>
#include <unistd.h> 

bool file_exists(const std::string& path);
std::string get_mime_type(const std::string& path);
void parse_http_request(const std::string& request, std::string& method, std::string& path, std::string& version);
const LocationConfig* match_location(const std::vector<LocationConfig>& locations, const std::string& path);
bool is_cgi_request(const LocationConfig& loc, const std::string& uri);
std::string resolve_script_path(const std::string& uri, const LocationConfig& loc);
std::string decode_chunked_body(const std::string& chunked);

#endif
