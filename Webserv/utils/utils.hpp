#ifndef UTILS_HPP
#define UTILS_HPP

#include "LocationConfig.hpp"
#include "Config.hpp"
#include "../logger/Logger.hpp"
#include <map>          // add this
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <dirent.h>

// class Request;          // forward declare

bool file_exists(const std::string &path);
std::string get_mime_type(const std::string &path);
// void parse_http_request(const std::string& request, std::string& method, std::string& path, std::string& version);
const LocationConfig *match_location(const std::vector<LocationConfig> &locations, const std::string &path);
bool is_cgi_request(const LocationConfig &loc, const std::string &uri);
// std::string resolve_script_path(const std::string& uri, const LocationConfig& loc);
std::string decode_chunked_body(const std::string &body);
bool is_directory(const std::string &path);
std::string generate_directory_listing(const std::string &dir_path, const std::string &uri_path);
std::string sanitize_filename(const std::string& in);
void split_basename_ext(const std::string& name, std::string& base, std::string& ext);
std::string get_boundary_from_content_type(const std::string& contentType);
bool extract_multipart_file_raw(const std::string& body, const std::string& boundary, std::string& outFilename, std::string& outContent);
bool has_chunked_encoding(const std::string& headers);
size_t find_chunked_terminator(const std::string& buf, size_t body_start);
bool wants_json(const Request& req); // JESS: json response from server helper
std::map<std::string,std::string> json_headers();  // JESS: json response from server helper

#endif
