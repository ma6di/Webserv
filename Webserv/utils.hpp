#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>
#include <dirent.h>
#include <vector>

bool file_exists(const std::string& path);
std::string get_mime_type(const std::string& path);
std::string generate_directory_listing(const std::string& dir_path, const std::string& request_path); 
bool is_method_allowed(const std::string& method, const std::vector<std::string>& allowed);

#endif