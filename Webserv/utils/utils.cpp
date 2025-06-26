#include "utils.hpp"

bool file_exists(const std::string& path) {
    struct stat buffer;
    bool exists = (stat(path.c_str(), &buffer) == 0);
    Logger::log(LOG_DEBUG, "file_exists", "Checked: " + path + " exists=" + (exists ? "true" : "false"));
    return exists;
}

std::string get_mime_type(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        Logger::log(LOG_DEBUG, "get_mime_type", "No extension for: " + path);
        return "application/octet-stream";
    }

    std::string ext = path.substr(dot);
    std::string mime;

    if (ext == ".html")
        mime = "text/html";
    else if (ext == ".css")
        mime = "text/css";
    else if (ext == ".js")
        mime = "application/javascript";
    else if (ext == ".txt")
        mime = "text/plain";
    else if (ext == ".jpg" || ext == ".jpeg")
        mime = "image/jpeg";
    else if (ext == ".png")
        mime = "image/png";
    else if (ext == ".gif")
        mime = "image/gif";
    else
        mime = "application/octet-stream";

    Logger::log(LOG_DEBUG, "get_mime_type", "Path: " + path + " -> " + mime);
    return mime;
}

const LocationConfig* match_location(const std::vector<LocationConfig>& locations, const std::string& path) {
    const LocationConfig* bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < locations.size(); ++i) {
        const std::string& locPath = locations[i].path;
        if (path.find(locPath) == 0 && locPath.length() > bestLength) {
            bestLength = locPath.length();
            bestMatch = &locations[i];
        }
    }

    if (bestMatch)
        Logger::log(LOG_DEBUG, "match_location", "Matched: " + bestMatch->path + " for path: " + path);
    else
        Logger::log(LOG_DEBUG, "match_location", "No match for path: " + path);

    return bestMatch;
}

bool is_cgi_request(const LocationConfig& loc, const std::string& uri) {
    std::string cgi_uri = loc.path;  // e.g. /cgi-bin
    std::string cgi_root = loc.root; // e.g. www/cgi-bin

    if (uri.find(cgi_uri) != 0)
        return false;

    // Get the path after the location prefix
    std::string rel_uri = uri.substr(cgi_uri.length());
    if (!rel_uri.empty() && rel_uri[0] == '/')
        rel_uri = rel_uri.substr(1);

    // Only check the first segment as the script
    size_t slash = rel_uri.find('/');
    std::string script_name = (slash == std::string::npos) ? rel_uri : rel_uri.substr(0, slash);
    if (script_name.empty())
        return false;

    std::string abs_script = cgi_root;
    if (!abs_script.empty() && abs_script[abs_script.size() - 1] != '/')
        abs_script += "/";
    abs_script += script_name;

    Logger::log(LOG_DEBUG, "is_cgi_request", "abs_script: [" + abs_script + "]");
    bool valid = file_exists(abs_script) && access(abs_script.c_str(), X_OK) == 0;
    Logger::log(LOG_DEBUG, "is_cgi_request", std::string("CGI valid: ") + (valid ? "true" : "false"));
    return valid;
}

std::string decode_chunked_body(const std::string& body) {
    std::istringstream in(body);
    std::string decoded, line;
    while (std::getline(in, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty())
            continue;
        // Parse chunk size (hex)
        size_t chunk_size = 0;
        std::istringstream chunk_size_stream(line);
        chunk_size_stream >> std::hex >> chunk_size;
        if (chunk_size == 0)
            break;
        // Read chunk data
        std::string chunk(chunk_size, '\0');
        in.read(&chunk[0], chunk_size);
        decoded += chunk;
        // Read the trailing \r\n after chunk data
        std::getline(in, line);
    }
    Logger::log(LOG_DEBUG, "decode_chunked_body", "Decoded chunked body, size=" + to_str(decoded.size()));
    return decoded;
}

bool is_directory(const std::string& path) {
    struct stat statbuf;
    bool dir = stat(path.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
    Logger::log(LOG_DEBUG, "is_directory", path + " is_directory=" + (dir ? "true" : "false"));
    return dir;
}

std::string generate_directory_listing(const std::string& dir_path, const std::string& uri_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        Logger::log(LOG_ERROR, "generate_directory_listing", "Failed to open dir: " + dir_path);
        return "<html><body><h1>403 Forbidden</h1></body></html>";
    }

    std::ostringstream html;
    html << "<html><head><title>Index of " << uri_path << "</title></head><body>";
    html << "<h1>Index of " << uri_path << "</h1><ul>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;

        html << "<li><a href=\"" << uri_path;
        if (!uri_path.empty() && uri_path[uri_path.length() - 1] != '/') html << "/";
        html << name << "\">" << name << "</a></li>";
    }

    html << "</ul></body></html>";
    closedir(dir);
    Logger::log(LOG_DEBUG, "generate_directory_listing", "Generated listing for: " + dir_path);
    return html.str();
}
