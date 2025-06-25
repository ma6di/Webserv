#include "WebServer.hpp"

extern Config g_config;

// --- GET Handler ---
void WebServer::handle_get(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "GET");
    std::cout << "[DEBUG] handle_get: uri=" << uri << " path=" << path << std::endl;

    if (loc && !loc->redirect_url.empty()) {
        send_redirect_response(client_fd, loc->redirect_code, loc->redirect_url, i);
        return;
    }

    if (is_directory(path)) {
        handle_directory_request(path, uri, loc, client_fd, i);
        return;
    }

    handle_file_request(path, client_fd, i);
}

// --- Directory Handler ---
void WebServer::handle_directory_request(const std::string& path, const std::string& uri, const LocationConfig* loc, int client_fd, size_t i) {
    std::string index_path = path + "/index.html";
    if (file_exists(index_path)) {
        send_file_response(client_fd, index_path, i);
        return;
    }
    if (loc && loc->autoindex) {
        std::string html = generate_directory_listing(path, uri);
        send_ok_response(client_fd, html, content_type_html(), i);
        return;
    }
    send_error_response(client_fd, 403, "Forbidden", i);
}

// --- File Handler ---
void WebServer::handle_file_request(const std::string& path, int client_fd, size_t i) {
    if (!file_exists(path)) {
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }
    if (access(path.c_str(), R_OK) != 0) {
        send_error_response(client_fd, 403, "Forbidden", i);
        return;
    }
    send_file_response(client_fd, path, i);
}

// --- CGI Handler ---
void WebServer::handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string method = request.getMethod();
    std::string cgi_root = loc->root;
    std::string cgi_uri = loc->path;
    std::string script_path, script_name, path_info;
    size_t match_len = 0;

    if (uri.find(cgi_uri) != 0) {
        send_error_response(client_fd, 404, "CGI Script Not Found", i);
        return;
    }
    std::string rel_uri = uri.substr(cgi_uri.length());

    // Find the longest matching script file in cgi_root
    for (size_t pos = rel_uri.size(); pos > 0; --pos) {
        if (rel_uri[pos - 1] == '/')
            continue;
        std::string candidate = rel_uri.substr(0, pos);
        std::string abs_candidate = cgi_root + candidate;
        if (file_exists(abs_candidate) && access(abs_candidate.c_str(), X_OK) == 0) {
            script_path = abs_candidate;
            script_name = cgi_uri + candidate;
            match_len = pos;
            break;
        }
    }

    if (script_path.empty()) {
        send_error_response(client_fd, 404, "CGI Script Not Found", i);
        return;
    }

    path_info = rel_uri.substr(match_len);

    std::map<std::string, std::string> env;
    env["REQUEST_METHOD"] = method;
    size_t q = uri.find('?');
    std::string query_string = q == std::string::npos ? "" : uri.substr(q + 1);
    env["SCRIPT_NAME"] = script_name;
    env["QUERY_STRING"] = query_string;
    env["PATH_INFO"] = path_info;
    if (method == "POST") {
        std::ostringstream oss;
        oss << request.getBody().size();
        env["CONTENT_LENGTH"] = oss.str();
        env["CONTENT_TYPE"] = request.getHeader("Content-Type");
    }
    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"] = "HTTP/1.1";
    env["SERVER_SOFTWARE"] = "Webserv/1.0";
    env["REDIRECT_STATUS"] = "200";

    CGIHandler handler(script_path, env, request.getBody(), request.getPath());
    std::string cgi_output = handler.execute();

    if (cgi_output == "__CGI_TIMEOUT__") {
        send_error_response(client_fd, 504, "Gateway Timeout", i);
        return;
    } else if (cgi_output == "__CGI_MISSING_HEADER__") {
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    size_t header_end = cgi_output.find("\r\n\r\n");
    size_t sep_len = 4;
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        sep_len = 2;
    }
    if (header_end == std::string::npos) {
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    std::string headers = cgi_output.substr(0, header_end);
    std::string body = cgi_output.substr(header_end + sep_len);

    std::map<std::string, std::string> cgi_headers;
    std::istringstream header_stream(headers);
    std::string line;
    bool has_content_type = false;
    while (std::getline(header_stream, line)) {
        if (line.empty() || line == "\r")
            continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
            std::string key_lower = key;
            for (size_t i = 0; i < key_lower.length(); ++i)
                key_lower[i] = std::tolower(static_cast<unsigned char>(key_lower[i]));
            if (key_lower == "content-type")
                has_content_type = true;
            cgi_headers[key] = value;
        }
    }
    if (!has_content_type) {
        cgi_headers["Content-Type"] = "text/html";
    }

    send_ok_response(client_fd, body, cgi_headers, i);
}

// --- POST Handler ---
void WebServer::handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "POST");
    std::cout << "[DEBUG] handle_post: method=" << request.getMethod() << ", path=" << request.getPath() << std::endl;
    std::cout << "[DEBUG] handle_post: uri=" << uri << " path=" << path << std::endl;

    if (loc && is_cgi_request(*loc, request.getPath())) {
        handle_cgi(loc, request, client_fd, i);
        return;
    }

    if (handle_upload(request, loc, client_fd, i)) {
        return;
    }

    if (file_exists(path)) {
        if (access(path.c_str(), W_OK) != 0) {
            send_error_response(client_fd, 403, "Forbidden", i);
            return;
        }
        // Optionally: handle file update logic here
    } else {
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }

    send_error_response(client_fd, 400, "Bad POST Request", i);
}

// --- DELETE Handler ---
void WebServer::handle_delete(const Request& request, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "DELETE");
    if (!file_exists(path)) {
        send_error_response(client_fd, 404, "Not Found", i);
    } else if (access(path.c_str(), W_OK) != 0) {
        send_error_response(client_fd, 403, "Forbidden", i);
    } else if (remove(path.c_str()) == 0) {
        send_ok_response(client_fd, "<html><body><h1>File deleted: " + uri + "</h1></body></html>", content_type_html(), i);
    } else {
        send_error_response(client_fd, 500, "Internal Server Error", i);
    }
}

// --- UPLOAD Helpers (unchanged, but could be moved to upload.cpp) ---
std::string extract_file_from_multipart(const std::string& body, std::string& filename) {
    std::istringstream stream(body);
    std::string line;
    bool in_headers = true;
    bool in_content = false;
    std::ostringstream file_content;

    filename = "upload";  // default fallback

    while (std::getline(stream, line)) {
        // Remove \r from the end if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        // Boundary or end
        if (line.find("------") == 0) {
            if (in_content) break;
            continue;
        }

        // Extract filename
        if (line.find("Content-Disposition:") != std::string::npos) {
            size_t pos = line.find("filename=");
            if (pos != std::string::npos) {
                size_t start = line.find('"', pos);
                size_t end = line.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    filename = line.substr(start + 1, end - start - 1);
                }
            }
        }

        // Headers done, content starts after blank line
        if (line.empty() && in_headers) {
            in_headers = false;
            in_content = true;
            continue;
        }

        if (in_content)
            file_content << line << "\n";
    }

    // Remove last newline
    std::string content = file_content.str();
    if (!content.empty() && content[content.size() - 1] == '\n')
        content.erase(content.size() - 1);

    return content;
}

bool WebServer::handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    if (!is_valid_upload_request(request, loc)) {
        std::cout << "🔍 not an upload:\n";
        return false;
    }

    std::string filename, content;
    process_upload_content(request, filename, content);

    // --- NEW: Use URI filename if present ---
    std::string uri = request.getPath(); // e.g. /upload/test_forbidden.txt
    std::string upload_dir = loc->upload_dir;
    std::string target_path;

    if (uri.length() > loc->path.length()) {
        // /upload/filename
        std::string uri_filename = uri.substr(loc->path.length());
        // Remove leading slash if present
        if (!uri_filename.empty() && uri_filename[0] == '/')
            uri_filename = uri_filename.substr(1);
        target_path = upload_dir + "/" + uri_filename;
    } else {
        // POST to /upload, use generated filename
        target_path = upload_dir + "/" + make_upload_filename(filename);
    }

    // --- Check if file exists and is writable ---
    if (file_exists(target_path) && access(target_path.c_str(), W_OK) != 0) {
        std::cerr << "❌ Forbidden: cannot write to " << target_path << "\n";
        send_error_response(client_fd, 403, "Forbidden", i);
        return true;
    }

    if (!write_upload_file(target_path, content)) {
        std::cerr << "❌ Failed to open file: " << target_path << "\n";
        send_error_response(client_fd, 500, "Failed to save upload", i);
        return true;
    }

    send_upload_success_response(client_fd, target_path, i);
    return true;
}

// --- Helper functions ---

bool WebServer::is_valid_upload_request(const Request& request, const LocationConfig* loc) {
    return request.getMethod() == "POST" && loc && !loc->upload_dir.empty();
}

void WebServer::process_upload_content(const Request& request, std::string& filename, std::string& content) {
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.find("multipart/form-data") != std::string::npos) {
        std::cout << "🔍 Detected multipart upload\n";
        content = extract_file_from_multipart(request.getBody(), filename);
        size_t slash = filename.find_last_of("/\\");
        if (slash != std::string::npos)
            filename = filename.substr(slash + 1);
    } else {
        std::cout << "🔍 Detected non-multipart upload\n";
        content = request.getBody();
        filename = "upload";
    }
}

std::string WebServer::make_upload_filename(const std::string& filename) {
    return filename + "_" + timestamp() + ".txt";
}

bool WebServer::write_upload_file(const std::string& full_path, const std::string& content) {
    std::ofstream out(full_path.c_str(), std::ios::binary);
    if (!out)
        return false;
    out << content;
    out.close();
    return true;
}



std::string WebServer::timestamp() {
    time_t now = time(NULL);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}
