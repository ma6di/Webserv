#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>

bool file_exists(const std::string& path);
std::string get_mime_type(const std::string& path);
void parse_http_request(const std::string& request, std::string& method, std::string& path, std::string& version);

#endif