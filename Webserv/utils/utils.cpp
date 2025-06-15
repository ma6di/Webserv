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

    std::cout << "[DEBUG] abs_script: [" << abs_script << "]\n";
	
    return file_exists(abs_script) && access(abs_script.c_str(), X_OK) == 0;
}

std::string resolve_script_path(const std::string& uri, const LocationConfig& loc) {
    std::string root = loc.root.empty() ? "./www" : loc.root;
    return root + uri.substr(loc.path.length());  // Trim location prefix from URI
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
    return decoded;
}
