#include "utils.hpp"
#include <sstream>

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

void    parse_http_request(const std::string& request, std::string& method, std::string& path, std::string& version) {
    std::istringstream stream(request);
    std::string request_line;

    if (!std::getline(stream, request_line)) {
        throw std::runtime_error("Empty request");
    }

    if (!request_line.empty() && request_line[request_line.size() - 1] == '\r') {
        request_line.erase(request_line.size() - 1);
    }

    std::istringstream line_stream(request_line);
    line_stream>> method >> path >> version;

    std::cout << "Parsed parts:\n";
    std::cout << "  Method: [" << method << "]\n";
    std::cout << "  Path: [" << path << "]\n";
    std::cout << "  Version: [" << version << "]\n";

    if (method.empty() || path.empty() || version.empty()) {
        throw std::runtime_error("Invalid HTTP request line");
    }
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

    return bestMatch;
}

bool is_cgi_request(const LocationConfig& loc, const std::string& uri) {
    std::string cgi_root = loc.root; // e.g. ./www/cgi-bin/
    std::string cgi_uri = loc.path;  // e.g. /cgi-bin/
    if (uri.find(cgi_uri) != 0)
        return false;
    std::string rel_uri = uri.substr(cgi_uri.length()); // e.g. test.py/foo/bar
    for (size_t pos = rel_uri.size(); pos > 0; --pos) {
        if (rel_uri[pos - 1] == '/')
            continue;
        std::string candidate = rel_uri.substr(0, pos); // e.g. test.py
        std::string abs_candidate = cgi_root + candidate;
        if (file_exists(abs_candidate) && access(abs_candidate.c_str(), X_OK) == 0)
            return true;
    }
    return false;
}

std::string resolve_script_path(const std::string& uri, const LocationConfig& loc) {
    std::string root = loc.root.empty() ? "./www" : loc.root;
    return root + uri.substr(loc.path.length());  // Trim location prefix from URI
}

std::string decode_chunked_body(const std::string& chunked) {
    std::istringstream stream(chunked);
    std::string decoded, line;
    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r")
            continue;
        size_t chunk_size = 0;
        std::istringstream size_stream(line);
        size_stream >> std::hex >> chunk_size;
        if (chunk_size == 0)
            break;
        std::string chunk(chunk_size, '\0');
        stream.read(&chunk[0], chunk_size);
        decoded += chunk;
        // Skip the trailing \r\n after the chunk
        stream.get();
        if (stream.peek() == '\n') stream.get();
    }
    return decoded;
}
