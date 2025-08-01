#include "WebServer.hpp"

extern Config g_config;

/*std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method, const LocationConfig* loc) {
    std::string path = raw_path;
    std::string base_dir;
    std::string resolved_path;
    (void)loc; // Avoid unused parameter warning
    Logger::log(LOG_DEBUG, "resolve_path", "raw_path = \"" + raw_path + "\", method = " + method);

    // 1. CGI
    if (path.compare(0, 8, "/cgi-bin") == 0 && (path.size() == 8 || path[8] == '/')) {
        base_dir = "./www/cgi-bin";
        resolved_path = base_dir + path.substr(8);
        if (resolved_path.empty() || resolved_path == "/")
            resolved_path = base_dir + "/index.py";
        Logger::log(LOG_DEBUG, "resolve_path", "CGI path resolved: " + resolved_path);
        return resolved_path;
    }

    // 2. Root
    if (path == "/" || path.empty()) {
        base_dir = "./www/static";
        resolved_path = base_dir + "/index.html";
        Logger::log(LOG_DEBUG, "resolve_path", "Root GET, serving static index: " + resolved_path);
        return resolved_path;
    }

    // 3. Upload
    if (path.compare(0, 7, "/upload") == 0 && (path.size() == 7 || path[7] == '/')) {
        base_dir = "./www/upload";
        resolved_path = base_dir + path.substr(7);
        Logger::log(LOG_DEBUG, "resolve_path", "Upload path resolved: " + resolved_path);
        return resolved_path;
    }
    // 4. Static
    if (path.compare(0, 7, "/static") == 0 && (path.size() == 7 || path[7] == '/')) {
        base_dir = "./www/static";
        resolved_path = base_dir + path.substr(7);
        if (resolved_path == base_dir)
            resolved_path += "/index.html";
        if (resolved_path == base_dir + "/")
            resolved_path += "index.html";
        Logger::log(LOG_DEBUG, "resolve_path", "Static path resolved: " + resolved_path);
        return resolved_path;
    }

    // 5. Default: treat as static file
    base_dir = "./www/static";
    resolved_path = base_dir + path;
    if (!resolved_path.empty() && resolved_path[resolved_path.size() - 1] == '/')
        resolved_path += "/index.html";
    Logger::log(LOG_DEBUG, "resolve_path", "Default static path: " + resolved_path);
    return resolved_path;
}*/

/*std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method, const LocationConfig* loc) {
    Logger::log(LOG_DEBUG, "resolve_path", "raw_path = \"" + raw_path + "\", method = " + method);

    if (loc) {
        std::string base_dir = loc->root;
        std::string rel_path = raw_path.substr(loc->path.length()); // Strip the location prefix
        if (!rel_path.empty() && rel_path[0] == '/')
            rel_path = rel_path.substr(1);

        std::string resolved_path = base_dir;
        if (!rel_path.empty())
            resolved_path += "/" + rel_path;
        else
            resolved_path += "/" + loc->index; // default index file

        // If it's a directory, append index
        if (resolved_path[resolved_path.size() - 1] == '/')
            resolved_path += loc->index;

        Logger::log(LOG_DEBUG, "resolve_path", "Resolved with loc: " + resolved_path);
        return resolved_path;
    }

    // Fallback behavior (hardcoded)
    std::string fallback = "./www/static" + raw_path;
    if (!fallback.empty() && fallback[fallback.size() - 1] == '/')
        fallback += "index.html";

    Logger::log(LOG_DEBUG, "resolve_path", "Resolved fallback: " + fallback);
    return fallback;
}*/

/*std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method, const LocationConfig* loc) {
    Logger::log(LOG_DEBUG, "resolve_path", "raw_path = \"" + raw_path + "\", method = " + method);

    if (loc) {
        std::string base_dir = loc->root;
        std::string rel_path = raw_path;

        // Only strip prefix if not "/"
        if (loc->path != "/" && raw_path.find(loc->path) == 0)
            rel_path = raw_path.substr(loc->path.length());

        if (!rel_path.empty() && rel_path[0] == '/')
            rel_path = rel_path.substr(1);

        std::string resolved_path = base_dir;
        if (!rel_path.empty())
            resolved_path += "/" + rel_path;

        // Check if the resolved path is a directory
        if (is_directory(resolved_path)) {
            Logger::log(LOG_DEBUG, "resolve_path", "Path is a directory, appending index: " + loc->index);
            resolved_path += "/" + loc->index;
        }

        Logger::log(LOG_DEBUG, "resolve_path", "Resolved with loc: " + resolved_path);
        return resolved_path;
    }

    // Fallback behavior
    std::string fallback = "./www" + raw_path;
    if (is_directory(fallback))
        fallback += "/index.html";

    Logger::log(LOG_DEBUG, "resolve_path", "Resolved fallback: " + fallback);
    return fallback;
}*/

/*std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method, const LocationConfig* loc) {
    Logger::log(LOG_DEBUG, "resolve_path", "raw_path = \"" + raw_path + "\", method = " + method);

    if (loc) {
        std::string base_dir = loc->root;
        std::string rel_path = raw_path;

        // Strip location prefix
        if (loc->path != "/" && raw_path.find(loc->path) == 0)
            rel_path = raw_path.substr(loc->path.length());

        if (!rel_path.empty() && rel_path[0] == '/')
            rel_path = rel_path.substr(1);

        std::string resolved_path = base_dir;
        if (!rel_path.empty())
            resolved_path += "/" + rel_path;

        // DO NOT append index.html here.
        Logger::log(LOG_DEBUG, "resolve_path", "Resolved with loc: " + resolved_path);
        return resolved_path;
    }

    // Fallback
    std::string fallback = "./www" + raw_path;
    Logger::log(LOG_DEBUG, "resolve_path", "Resolved fallback: " + fallback);
    return fallback;
}*/

std::string WebServer::resolve_path(const std::string& raw_path,
                                    const std::string& method,
                                    const LocationConfig* loc) {
    Logger::log(LOG_DEBUG, "resolve_path",
                "raw_path = \"" + raw_path + "\", method = " + method);

    // Helper to strip the location prefix
    std::string rel = raw_path;
    if (loc && loc->path != "/" && raw_path.find(loc->path) == 0) {
        rel = raw_path.substr(loc->path.length());
    }
    if (!rel.empty() && rel[0] == '/')
        rel = rel.substr(1);

    // Build candidate path under this location’s root
    std::string candidate;
    if (loc) {
        candidate = loc->root;
    } else {
        candidate = "./www";  // global fallback
    }
    if (!rel.empty())
        candidate += "/" + rel;

    Logger::log(LOG_DEBUG, "resolve_path", "Candidate path: " + candidate);

    // 1) If it’s a directory, return it—handle_get will dispatch to handle_directory_request()
    if (is_directory(candidate)) {
        Logger::log(LOG_DEBUG, "resolve_path", "Directory detected, returning: " + candidate);
        return candidate;
    }

    // 2) If it’s a file, return it directly
    if (file_exists(candidate)) {
        Logger::log(LOG_DEBUG, "resolve_path", "File exists, returning: " + candidate);
        return candidate;
    }

    // 3) Fallback: try adding “.html”
    std::string html_fallback = candidate + ".html";
    if (file_exists(html_fallback)) {
        Logger::log(LOG_DEBUG, "resolve_path", "HTML fallback, returning: " + html_fallback);
        return html_fallback;
    }

    // 4) Nothing matched — return the original candidate (so handle_file_request will 404)
    Logger::log(LOG_DEBUG, "resolve_path", "Nothing found, returning: " + candidate);
    return candidate;
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
    if (!file) {
        Logger::log(LOG_ERROR, "read_file", "Failed to open file: " + path);
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WebServer::read_and_append_client_data(int client_fd, size_t i) {
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        // Client disconnected or error
        Logger::log(LOG_INFO, "read_and_append_client_data", "Client disconnected or error on FD=" + to_str(client_fd));
        cleanup_client(client_fd, i);
        return false;
    }
    client_buffers[client_fd].append(buffer, bytes_read);

    // Check if buffer is too large (AFTER appending)
    if (client_buffers[client_fd].size() > g_config.getMaxBodySize()) {
        Logger::log(LOG_ERROR, "read_and_append_client_data", "Payload Too Large for FD=" + to_str(client_fd));
        send_error_response(client_fd, 413, "Payload Too Large", i);
        usleep(100000); // 100ms
        return false;
    }

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

// --- Helper: Check if full body is received ---
bool WebServer::is_full_body_received(const Request& request, const std::string& request_data, size_t header_end) {
    bool is_chunked = request.isChunked();
    int content_length = request.getContentLength();
    size_t body_start = header_end + 4;
    size_t body_length_received = request_data.size() - body_start;

    if (!is_chunked && content_length > 0 && body_length_received < static_cast<size_t>(content_length)) {
        return false;
    }
    if (is_chunked && request.getBody().empty()) {
        return false;
    }
    return true;
}
