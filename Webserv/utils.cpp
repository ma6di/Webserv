#include "utils.hpp"

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string get_mime_type(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return ("application/octet-stream");

    std::string ext = path.substr(dot);

    if (ext == ".html")
        return "text/html";
    if (ext == ".css")
        return "text/css";        
    if (ext == ".js")
        return "application/javascript";
    if (ext == ".txt")
        return "text/plain";        
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";   
    if (ext == ".png")
        return "image/png";        
    if (ext == ".gif")
        return "image/gif";
    return "application/octet-stream";
}

std::string generate_directory_listing(const std::string& dir_path, const std::string& request_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir)
        return "<h1>403 Forbidden</h1>";
    
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html><head><meta charset=\"UTF-8\"><title>Directory Listing</title></head><body>\n";
    html << "<h1>Directory Listing for " << request_path << "</h1>\n";
    html << "<ul>\n";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        if (name.empty())
            continue;
        html << "<li><a href=\"" << request_path;
        if (request_path[request_path.size() - 1] != '/')
            html << "/";
        html << name << "\">" << name << "</a></li>\n";
    }

    closedir(dir);
    html << "</ul></body></html>";
    return html.str();
}

bool is_method_allowed(const std::string& method, const std::vector<std::string>& allowed) {
    for (size_t i = 0; i < allowed.size(); ++i) {
        if (allowed[i] == method)
            return true;
    }
    return false;
}
