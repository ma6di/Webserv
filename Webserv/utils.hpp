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

#endif