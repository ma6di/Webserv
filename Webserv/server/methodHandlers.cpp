#include "WebServer.hpp"
#include "CGIHandler.hpp"

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
    std::string script_path, script_name, path_info;
    if (!CGIHandler::find_cgi_script(loc->root, loc->path, request.getPath(), script_path, script_name, path_info)) {
        send_error_response(client_fd, 404, "CGI Script Not Found", i);
        return;
    }

    std::map<std::string, std::string> env = CGIHandler::build_cgi_env(request, script_name, path_info);
    CGIHandler handler(script_path, env, request.getBody(), request.getPath());
    std::string cgi_output = handler.execute();

    if (cgi_output == "__CGI_TIMEOUT__") {
        send_error_response(client_fd, 504, "Gateway Timeout", i);
        return;
    }
    if (cgi_output == "__CGI_MISSING_HEADER__") {
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    std::map<std::string, std::string> cgi_headers;
    std::string body;
    CGIHandler::parse_cgi_output(cgi_output, cgi_headers, body);
    if (cgi_headers.empty() && body.empty()) {
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
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
