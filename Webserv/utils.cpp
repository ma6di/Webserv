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
