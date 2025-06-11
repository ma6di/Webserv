#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"

std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method) {
    std::string path = raw_path;
    std::string base_dir;
    std::string resolved_path;

    std::cout << "[DEBUG] resolve_path: raw_path = \"" << raw_path << "\", method = " << method << std::endl;

    // 1. CGI: If path is /cgi-bin or matches your CGI config
    if (path.find("/cgi-bin") == 0) {
        base_dir = "./www/cgi-bin";
        resolved_path = base_dir + path.substr(8); // remove "/cgi-bin" prefix
        if (resolved_path.empty() || resolved_path == "/")
            resolved_path = base_dir + "/index.py"; // default CGI script
        std::cout << "[DEBUG] CGI path resolved: " << resolved_path << std::endl;
        return resolved_path;
    }

    // 2. POST/DELETE/GET with no directory specified (e.g. "/")
    if (path == "/" || path.empty()) {
        if (method == "POST" || method == "DELETE") {
            base_dir = "./www/upload";
            resolved_path = base_dir; // Could be a directory or upload handler
            std::cout << "[DEBUG] Root POST/DELETE, using upload dir: " << resolved_path << std::endl;
            return resolved_path;
        } else { // GET
            base_dir = "./www/static";
            resolved_path = base_dir + "/index.html";
            std::cout << "[DEBUG] Root GET, serving static index: " << resolved_path << std::endl;
            return resolved_path;
        }
    }

    // 3. If path starts with /upload
    if (path.find("/upload") == 0) {
        base_dir = "./www/upload";
        resolved_path = base_dir + path.substr(7); // remove "/upload" prefix
        if (resolved_path == base_dir || resolved_path == base_dir + "/")
            resolved_path += "index.html";
        std::cout << "[DEBUG] Upload path resolved: " << resolved_path << std::endl;
        return resolved_path;
    }

    // 4. If path starts with /static
    if (path.find("/static") == 0) {
        base_dir = "./www/static";
        resolved_path = base_dir + path.substr(7); // remove "/static" prefix
        if (resolved_path == base_dir || resolved_path == base_dir + "/")
            resolved_path += "index.html";
        std::cout << "[DEBUG] Static path resolved: " << resolved_path << std::endl;
        return resolved_path;
    }

    // 5. DELETE/POST/GET with a directory specified (e.g. /somefolder/file.txt)
    // Here you should check your config/location objects to see if the method is allowed.
    // For now, assume static for GET, upload for POST/DELETE if not matched above.
    if (method == "DELETE" || method == "POST") {
        // If the path matches a directory with allowed DELETE/POST, use that dir
        // (You may want to check your config here)
        base_dir = "./www/upload";
        resolved_path = base_dir + path;
        std::cout << "[DEBUG] POST/DELETE fallback to upload: " << resolved_path << std::endl;
        return resolved_path;
    }

    // 6. Default: treat as static file
    base_dir = "./www/static";
    resolved_path = base_dir + path;
	if (!file_exists(resolved_path) && !resolved_path.empty() && resolved_path[resolved_path.size() - 1] == '/')
		resolved_path += "index.html";

    std::cout << "[DEBUG] Default static path: " << resolved_path << std::endl;
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
