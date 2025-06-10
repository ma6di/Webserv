#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"

std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method) {
    std::string path = raw_path;
    std::string base_dir = "./www";
    std::string resolved_path;

    std::cout << "[DEBUG] resolve_path: raw_path = \"" << raw_path << "\", method = " << method << std::endl;

    // Redirect root requests based on method
    if (path == "/") {
        if (method == "POST" || method == "DELETE") {
            path = "/upload";
        } else {
            path = "/static/index.html";
        }
        std::cout << "[DEBUG] resolve_path: root path adjusted to \"" << path << "\"" << std::endl;
    }

    // Append index.html to paths ending with '/'
    if (!path.empty() && path[path.size() - 1] == '/') {
        path += "html/index.html";
        std::cout << "[DEBUG] resolve_path: trailing slash - path set to \"" << path << "\"" << std::endl;
    }

    // Construct full path to file
    resolved_path = base_dir + path;
    std::cout << "[DEBUG] resolve_path: checking resolved_path = \"" << resolved_path << "\"" << std::endl;

    if (file_exists(resolved_path)) {
        std::cout << "[DEBUG] resolve_path: file found - returning \"" << resolved_path << "\"" << std::endl;
        return resolved_path;
    }

    // Try appending .html if no file extension is present
    if (path.find('.') == std::string::npos) {
        std::string fallback_path = base_dir + path + ".html";
        std::cout << "[DEBUG] resolve_path: no extension, checking fallback = \"" << fallback_path << "\"" << std::endl;

        if (file_exists(fallback_path)) {
            std::cout << "[DEBUG] resolve_path: fallback found - returning \"" << fallback_path << "\"" << std::endl;
            return fallback_path;
        }
    }

    std::cout << "[DEBUG] resolve_path: file not found - returning original path \"" << resolved_path << "\"" << std::endl;
    return resolved_path;
}

std::string extract_filename(const std::string& header) {
    size_t pos = header.find("filename=");
    if (pos == std::string::npos)
        return "upload";

    size_t start = header.find('"', pos);
    size_t end = header.find('"', start + 1);
    if (start == std::string::npos || end == std::string::npos)
        return "upload";

    return header.substr(start + 1, end - start - 1);
}


// Reads the entire contents of a file into a string
std::string WebServer::read_file(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WebServer::read_and_append_client_data(int client_fd, size_t i) {
    char buf[1024];
    std::string& request_data = client_buffers[client_fd];
    int bytes = read(client_fd, buf, sizeof(buf) - 1);

    if (bytes <= 0) {
        std::cout << "[DEBUG] Client disconnected or read error on FD=" << client_fd << std::endl;
        cleanup_client(client_fd, i);
        return false;
    }

    request_data.append(buf, bytes);
    std::cout << "[DEBUG] request_data size after append: " << request_data.size() << std::endl;
    return true;
}

size_t WebServer::find_header_end(const std::string& request_data) {
    return request_data.find("\r\n\r\n");
}

int WebServer::parse_content_length(const std::string& headers) {
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("Content-Length:") != std::string::npos) {
            std::istringstream linestream(line);
            std::string key;
            int content_length = 0;
            linestream >> key >> content_length;
            return content_length;
        }
    }
    return 0;
}
