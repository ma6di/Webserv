#include "WebServer.hpp"

// This file contains HTTP method handlers for the WebServer class.
// Each handler processes a specific HTTP method (GET, POST, DELETE, CGI, etc.)
// and sends appropriate responses to the client.

// --- GET Handler ---

void WebServer::handle_get(const Request& req,
                           const LocationConfig* loc,
                           int client_fd,
                           size_t idx)
{
    // Resolve the filesystem path for the requested resource
    std::string fs_path = resolve_path(req.getPath(),
                                       req.getMethod(),
                                       loc);

    struct stat st;
    // Check if the file or directory exists
    if (stat(fs_path.c_str(), &st) < 0) {
        send_error_response(client_fd, 404, "Not Found", idx);
        return;
    }

    // If it's a directory, handle as directory; otherwise, handle as file
    if (S_ISDIR(st.st_mode)) {
        handle_directory_request(fs_path, req.getPath(), loc, client_fd, idx);
    }
    else {
        handle_file_request(fs_path, client_fd, idx);
    }
}

// --- Directory Handler ---

void WebServer::handle_directory_request(const std::string& path, const std::string& uri, const LocationConfig* loc, int client_fd, size_t i) {
    // Serve index file if present, otherwise generate autoindex or return forbidden
    // Use the configured index if set, otherwise default to index.html
    const std::string index_file = (loc && !loc->index.empty()) ? loc->index : "index.html";

    std::string index_path = path + "/" + index_file;
    // Serve index file if it exists
    if (file_exists(index_path)) {
        Logger::log(LOG_DEBUG, "handle_directory_request", "Serving index: " + index_path);
        send_file_response(client_fd, index_path, i);
        return;
    }
    // If autoindex is enabled, generate directory listing
    if (loc && loc->autoindex) {
        Logger::log(LOG_DEBUG, "handle_directory_request", "Autoindex enabled for: " + path);
        std::string html = generate_directory_listing(path, uri);
        send_ok_response(client_fd, html, content_type_html(), i);
        return;
    }
    // Otherwise, return forbidden
    Logger::log(LOG_ERROR, "handle_directory_request", "Forbidden: " + path);
    send_error_response(client_fd, 403, "Forbidden", i);
}

// --- File Handler ---
void WebServer::handle_file_request(const std::string& path, int client_fd, size_t i) {
    // Check if file exists
    if (!file_exists(path)) {
        Logger::log(LOG_ERROR, "handle_file_request", "File not found: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }
    // Check if file is readable
    if (access(path.c_str(), R_OK) != 0) {
        Logger::log(LOG_ERROR, "handle_file_request", "File not readable: " + path);
        send_error_response(client_fd, 403, "Forbidden", i);
        return;
    }
    // Serve the file
    Logger::log(LOG_INFO, "handle_file_request", "Serving file: " + path);
    send_file_response(client_fd, path, i);
}

// --- CGI Handler ---
void WebServer::handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i) {
    // Find the CGI script to execute
    std::string script_path, script_name, path_info;
    if (!CGIHandler::find_cgi_script(loc->root, loc->path, request.getPath(), script_path, script_name, path_info)) {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Script Not Found: " + request.getPath());
        send_error_response(client_fd, 404, "CGI Script Not Found", i);
        return;
    }

    // Build environment variables for CGI
    std::map<std::string, std::string> env = CGIHandler::build_cgi_env(request, script_name, path_info);
    // Create CGI handler and execute script
    CGIHandler handler(script_path, env, request.getBody(), request.getPath());
    std::string cgi_output = handler.execute();

    // Handle CGI errors (timeout, missing header, etc.)
    if (cgi_output == "__CGI_TIMEOUT__") {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Timeout: " + script_path);
        send_error_response(client_fd, 504, "Gateway Timeout", i);
        return;
    }
    if (cgi_output == "__CGI_MISSING_HEADER__") {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Missing Header: " + script_path);
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    // Parse CGI output into headers and body
    std::map<std::string, std::string> cgi_headers;
    std::string body;
    CGIHandler::parse_cgi_output(cgi_output, cgi_headers, body);
    if (cgi_headers.empty() && body.empty()) {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Output Empty: " + script_path);
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    // Send CGI response to client
    Logger::log(LOG_INFO, "handle_cgi", "CGI executed successfully: " + script_path);
    send_ok_response(client_fd, body, cgi_headers, i);
}

// --- POST Handler ---
void WebServer::handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    // Handle POST requests (file upload, CGI, etc.)
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "POST", loc);
    Logger::log(LOG_DEBUG, "handle_post", "method=" + request.getMethod() + ", uri=" + uri + " path=" + path);

    // If location is CGI, handle as CGI POST
    if (loc && is_cgi_request(*loc, request.getPath())) {
        Logger::log(LOG_DEBUG, "handle_post", "Detected CGI POST");
        handle_cgi(loc, request, client_fd, i);
        return;
    }

    // Try to handle as file upload
    if (handle_upload(request, loc, client_fd, i)) {
        Logger::log(LOG_INFO, "handle_post", "Handled as upload: " + uri);
        return;
    }

    // If file exists, check if writable
    if (file_exists(path)) {
        if (access(path.c_str(), W_OK) != 0) {
            Logger::log(LOG_ERROR, "handle_post", "File not writable: " + path);
            send_error_response(client_fd, 403, "Forbidden", i);
            return;
        }
        // Optionally: handle file update logic here
    } else {
        Logger::log(LOG_ERROR, "handle_post", "File not found: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }

    // If none of the above, return bad request
    Logger::log(LOG_ERROR, "handle_post", "Bad POST Request: " + uri);
    send_error_response(client_fd, 400, "Bad POST Request", i);
}

// --- DELETE Handler ---
void WebServer::handle_delete(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    // Handle DELETE requests (delete file)
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "DELETE", loc);
    Logger::log(LOG_DEBUG, "handle_delete", "uri=" + uri + " path=" + path);
    // Check if file exists and is writable, then delete
    if (!file_exists(path)) {
        Logger::log(LOG_ERROR, "handle_delete", "File not found: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
    } else if (access(path.c_str(), W_OK) != 0) {
        Logger::log(LOG_ERROR, "handle_delete", "File not writable: " + path);
        send_error_response(client_fd, 403, "Forbidden", i);
    } else if (remove(path.c_str()) == 0) {
        Logger::log(LOG_INFO, "handle_delete", "File deleted: " + path);
		send_no_content_response(client_fd, i);
        // send_ok_response(client_fd, "<html><body><h1>File deleted: " + uri + "</h1></body></html>", content_type_html(), i);
    } else {
        Logger::log(LOG_ERROR, "handle_delete", "Failed to delete file: " + path);
        send_error_response(client_fd, 500, "Internal Server Error", i);
    }
}

// --- UPLOAD Helpers (unchanged, but could be moved to upload.cpp) ---
std::string extract_file_from_multipart(const std::string& body, std::string& filename) {
    // Extract file content and filename from multipart/form-data body
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

    std::string content = file_content.str();
    if (!content.empty() && content[content.size() - 1] == '\n')
        content.erase(content.size() - 1);

    return content;
}

bool WebServer::handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    // Validate upload request and process file upload
    if (!is_valid_upload_request(request, loc)) {
        Logger::log(LOG_DEBUG, "is_valid_upload_request",
  "method=" + request.getMethod() +
  " upload_dir=" + (loc? loc->upload_dir : "<none>"));

        Logger::log(LOG_DEBUG, "handle_upload", "Not an upload request.");
        return false;
    }

    std::string filename, content;
    process_upload_content(request, filename, content);

    // --- NEW: Use URI filename if present ---
    std::string uri = request.getPath(); // e.g. /upload/test_forbidden.txt
    std::string upload_dir = loc->upload_dir;
    std::string target_path;

    // Determine target path for upload
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
        Logger::log(LOG_ERROR, "handle_upload", "Forbidden: cannot write to " + target_path);
        send_error_response(client_fd, 403, "Forbidden", i);
        return true;
    }

    // Write uploaded file to disk
    if (!write_upload_file(target_path, content)) {
        Logger::log(LOG_ERROR, "handle_upload", "Failed to open file: " + target_path);
        send_error_response(client_fd, 500, "Failed to save upload", i);
        return true;
    }

    Logger::log(LOG_INFO, "handle_upload", "Upload successful: " + target_path);
    send_upload_success_response(client_fd, target_path, i);
    return true;
}

// --- Helper functions ---

bool WebServer::is_valid_upload_request(const Request& request, const LocationConfig* loc) {
    // Check if request is a valid upload (POST and upload_dir is set)
    return request.getMethod() == "POST" && loc && !loc->upload_dir.empty();
}

/*void WebServer::process_upload_content(const Request& request, std::string& filename, std::string& content) {
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.find("multipart/form-data") != std::string::npos) {
        Logger::log(LOG_DEBUG, "process_upload_content", "Detected multipart upload");
        content = extract_file_from_multipart(request.getBody(), filename);
        size_t slash = filename.find_last_of("/\\");
        if (slash != std::string::npos)
            filename = filename.substr(slash + 1);
    } else {
        Logger::log(LOG_DEBUG, "process_upload_content", "Detected non-multipart upload");
        content = request.getBody();
        filename = "upload";
    }
}*/

void WebServer::process_upload_content(const Request& request,
                                       std::string& filename,
                                       std::string& content)
{
    // Parse upload content based on Content-Type (multipart or raw)
    std::string content_type = request.getHeader("Content-Type");

    if (content_type.find("multipart/form-data") != std::string::npos) {
        Logger::log(LOG_DEBUG, "process_upload_content", "Detected multipart upload");
        std::string boundary = get_boundary_from_content_type(content_type);

        std::string fn, data;
        if (extract_multipart_file_raw(request.getBody(), boundary, fn, data)) {
            filename = fn.empty() ? "upload" : fn;   // keep original extension!
            content  = data;                          // raw bytes
        } else {
            // Fallback: treat entire body as "raw"
            Logger::log(LOG_ERROR, "process_upload_content", "Multipart parse failed; using raw body");
            filename = "upload";
            content  = request.getBody();
        }
    } else {
        Logger::log(LOG_DEBUG, "process_upload_content", "Detected non-multipart upload");
        filename = "upload";
        content  = request.getBody(); // raw bytes already
    }
}


std::string WebServer::make_upload_filename(const std::string& filename) {
    // Sanitize filename and add timestamp for uniqueness
    std::string safe = sanitize_filename(filename.empty() ? "upload" : filename);

    std::string base, ext;
    split_basename_ext(safe, base, ext);

    // Insert timestamp before the extension
    return base + "_" + timestamp() + ext;
}

bool WebServer::write_upload_file(const std::string& full_path, const std::string& content) {
    // Write file content to disk
    std::ofstream out(full_path.c_str(), std::ios::binary);
    if (!out)
        return false;
    out << content;
    out.close();
    return true;
}

std::string WebServer::timestamp() {
    // Generate a timestamp string for filenames
    time_t now = time(NULL);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}
