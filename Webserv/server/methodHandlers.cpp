#include "WebServer.hpp"
#include "Connection.hpp"

// HTTP method handlers for WebServer. Each function processes a specific HTTP request type.

// --- GET Handler ---
// Handles HTTP GET requests: resolves path, serves file or directory, or sends 404.
void WebServer::handle_get(const Request& req,
                           const LocationConfig* loc,
                           int client_fd,
                           size_t idx)
{
    //JESS: just storing the req.getPath() in uri for cleaner code (instead of passing the fuction 3 times as an arg)
    std::string uri = req.getPath();
    std::string fs_path = resolve_path(uri,
                                       req.getMethod(),
                                       loc);

    /*stat is a system call that checks if a file or directory exists and gathers its metadata.
    If the path does not exist or cannot be accessed, stat returns a value less than 0.
    If stat succeeds, the st structure is filled with information about the file or directory
    (such as its type, permissions, size, etc.)*/
    struct stat st;
    if (stat(fs_path.c_str(), &st) < 0) {
        send_error_response(client_fd, 404, "Not Found", idx);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // JESS: if statement to check if get request comes from client
        if (wants_json(req))
        {
            std::string json = generate_directory_listing_json(fs_path);
            send_ok_response(client_fd, json, json_headers(), idx);
            std::cout << "SENT OK RESPONS JSON " << std::endl;
            return;
        }
        handle_directory_request(fs_path, uri, loc, client_fd, idx);
    }
    else {
        handle_file_request(fs_path, client_fd, idx);
    }
}

// --- Directory Handler ---
// Handles directory requests: serves index file, autoindex, or 403 Forbidden.
void WebServer::handle_directory_request(const std::string& path, const std::string& uri, const LocationConfig* loc, int client_fd, size_t i) {
    // Use the configured index if set, otherwise default to index.html
    const std::string index_file = (loc && !loc->index.empty()) ? loc->index : "index.html";

    std::string index_path = path + "/" + index_file;
    if (file_exists(index_path)) {
        //Logger::log(LOG_DEBUG, "handle_directory_request", "Serving index: " + index_path);
        send_file_response(client_fd, index_path, i);
        return;
    }
    if (loc && loc->autoindex) {
        //Logger::log(LOG_DEBUG, "handle_directory_request", "Autoindex enabled for: " + path);
        std::string html = generate_directory_listing(path, uri);
        send_ok_response(client_fd, html, content_type_html(), i);
        return;
    }
    Logger::log(LOG_ERROR, "handle_directory_request", "Forbidden: " + path);
    send_error_response(client_fd, 403, "Forbidden", i);
}

// --- File Handler ---
// Handles file requests: checks existence/readability, serves file or error.
void WebServer::handle_file_request(const std::string& path, int client_fd, size_t i) {
    if (!file_exists(path)) {
        Logger::log(LOG_ERROR, "handle_file_request", "File not found: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }
    if (access(path.c_str(), R_OK) != 0) {
        Logger::log(LOG_ERROR, "handle_file_request", "File not readable: " + path);
        send_error_response(client_fd, 403, "Forbidden", i);
        return;
    }
    Logger::log(LOG_INFO, "handle_file_request", "Serving file: " + path);
    send_file_response(client_fd, path, i);
}

// --- CGI Handler --- Common Gateway Interface
// Handles CGI requests: finds script, sets env, executes, parses output, sends response.
void WebServer::handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i) {
    std::string script_path, script_name, path_info;
    if (!CGIHandler::find_cgi_script(loc->root, loc->path, request.getPath(), script_path, script_name, path_info)) {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Script Not Found: " + request.getPath());
        send_error_response(client_fd, 404, "CGI Script Not Found", i);
        return;
    }

    std::map<std::string, std::string> env = CGIHandler::build_cgi_env(request, script_name, path_info);
    std::map<int, Connection> connections = this->getConnections();
    Connection &conn = connections[client_fd];
	CGIHandler handler(script_path, env, &conn, request.getBody(), request.getPath());    std::string cgi_output = handler.execute();

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

	if (cgi_output == "__CGI_INTERNAL_ERROR__") {
        Logger::log(LOG_ERROR, "502", "CGI Internal Error: " + script_path);
        send_error_response(client_fd, 502, "Bad Gateway", i);
        return;
    }

    std::map<std::string, std::string> cgi_headers;
    std::string body;
    CGIHandler::parse_cgi_output(cgi_output, cgi_headers, body);
    if (cgi_headers.empty() && body.empty()) {
        Logger::log(LOG_ERROR, "handle_cgi", "CGI Output Empty: " + script_path);
        send_error_response(client_fd, 500, "Internal Server Error", i);
        return;
    }

    Logger::log(LOG_INFO, "handle_cgi", "CGI executed successfully: " + script_path);
    send_ok_response(client_fd, body, cgi_headers, i);
}

// --- POST Handler ---
// Handles HTTP POST requests: supports CGI, file upload, file update, or error.
void WebServer::handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "POST", loc);
    //Logger::log(LOG_DEBUG, "handle_post", "method=" + request.getMethod() + ", uri=" + uri + " path=" + path);

    if (loc && is_cgi_request(*loc, request.getPath())) {
        //Logger::log(LOG_DEBUG, "handle_post", "Detected CGI POST");
        handle_cgi(loc, request, client_fd, i);
        return;
    }

    if (handle_upload(request, loc, client_fd, i)) {
        Logger::log(LOG_INFO, "handle_post", "Handled as upload: " + uri);
        return;
    }

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

    Logger::log(LOG_ERROR, "handle_post", "Bad POST Request: " + uri);
    send_error_response(client_fd, 400, "Bad POST Request", i);
}

void WebServer::handle_delete(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path;

    // If this location stores uploads in a dedicated dir, delete from there
    if (loc && !loc->upload_dir.empty()) {
        // derive basename from the URI relative to the location path
        std::string suffix;
        if (uri.size() > loc->path.size())
            suffix = uri.substr(loc->path.size());
        // strip one leading slash if present
        if (!suffix.empty() && suffix[0] == '/')
            suffix.erase(0, 1);

        // empty basename (e.g., DELETE /new_files/) is a bad request
        if (suffix.empty()) {
            Logger::log(LOG_ERROR, "handle_delete", "No filename in URL for delete: " + uri);
            send_error_response(client_fd, 400, "Bad Request", i);
            return;
        }
        // very light safety: disallow nested paths / traversal in the basename
        if (suffix.find('/') != std::string::npos || suffix.find("..") != std::string::npos) {
            Logger::log(LOG_ERROR, "handle_delete", "Invalid delete target: " + suffix);
            send_error_response(client_fd, 400, "Bad Request", i);
            return;
        }

        // join upload_dir + basename
        path = loc->upload_dir;
        if (!path.empty() && path[path.size() - 1] != '/')
            path += '/';
        path += suffix;
    } else {
        // default behavior for locations without upload_dir
        path = resolve_path(uri, "DELETE", loc);
    }

    //Logger::log(LOG_DEBUG, "handle_delete", "uri=" + uri + " path=" + path);

    if (!file_exists(path)) {
        Logger::log(LOG_ERROR, "handle_delete", "File not found: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
    } else if (is_directory(path)) {
        Logger::log(LOG_ERROR, "handle_delete", "Refusing to delete a directory: " + path);
        send_error_response(client_fd, 403, "Forbidden", i);
    } else if (access(path.c_str(), W_OK) != 0) {
        Logger::log(LOG_ERROR, "handle_delete", "File not writable: " + path);
        send_error_response(client_fd, 403, "Forbidden", i);
    } else if (remove(path.c_str()) == 0) {
        Logger::log(LOG_INFO, "handle_delete", "File deleted: " + path);
        send_no_content_response(client_fd, i);
    } else {
        Logger::log(LOG_ERROR, "handle_delete", "Failed to delete file: " + path);
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

    std::string content = file_content.str();
    if (!content.empty() && content[content.size() - 1] == '\n')
        content.erase(content.size() - 1);

    return content;
}

bool WebServer::handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    // Check if the request is a POST and the location config has an upload directory.
    if (!is_valid_upload_request(request, loc)) {
        /*Logger::log(LOG_DEBUG, "is_valid_upload_request",
            "method=" + request.getMethod() +
            " upload_dir=" + (loc? loc->upload_dir : "<none>"));
        Logger::log(LOG_DEBUG, "handle_upload", "Not an upload request.");*/
        return false;
    }

    // Extract the uploaded file’s name and content from the request body.
    std::string filename, content;
    process_upload_content(request, filename, content);

    std::string uri = request.getPath(); // e.g. /upload/test_forbidden.txt
    std::string upload_dir = loc->upload_dir;
    std::string base_filename;

    // If URI includes a filename (e.g., /upload/myfile.txt), use that as base
    if (uri.length() > loc->path.length()) {
        std::string uri_filename = uri.substr(loc->path.length());
        if (!uri_filename.empty() && uri_filename[0] == '/')
            uri_filename = uri_filename.substr(1);
        if (!uri_filename.empty()) {
            base_filename = uri_filename;
        } else if (!filename.empty()) {
            base_filename = make_upload_filename(filename);
        } else {
            base_filename = "upload";
        }
    } else {
        // POST to /upload, use multipart filename if available
        if (!filename.empty()) {
            base_filename = make_upload_filename(filename);
        } else {
            base_filename = make_upload_filename("upload");;
        }
    }

    // Always append timestamp to avoid overwrites
    std::string target_path = upload_dir + "/" + base_filename;

    // --- Check if file exists and is writable ---
    if (file_exists(target_path) && access(target_path.c_str(), W_OK) != 0) {
        Logger::log(LOG_ERROR, "handle_upload", "Forbidden: cannot write to " + target_path);
        send_error_response(client_fd, 403, "Forbidden", i);
        return true;
    }

    if (!write_upload_file(target_path, content)) {
        Logger::log(LOG_ERROR, "handle_upload", "Failed to open file: " + target_path);
        send_error_response(client_fd, 500, "Failed to save upload", i);
        return true;
    }

    Logger::log(LOG_INFO, "handle_upload", "Upload successful: " + target_path);
    /*
 JESS: added an if statement for the server to recognise when the post request
 is coming from the frontend, if that is the case it will send a json as a response.
 Added this change so the client can use the server directly instead of using the python script
 */
    if (wants_json(request))
    {
        send_upload_success_json(client_fd, target_path, i); // sends json response if request comes from client
    }
    else
    {
        send_upload_success_response(client_fd, target_path, i); // sends response when using curl, this was previous code
    }
    return true;
}

// --- Helper functions ---
// Checks if request is a valid upload (POST and upload_dir set).
bool WebServer::is_valid_upload_request(const Request& request, const LocationConfig* loc) {
    return request.getMethod() == "POST" && loc && !loc->upload_dir.empty();
}

// Processes upload content: extracts filename and file data from request body.
void WebServer::process_upload_content(const Request& request,
                                       std::string& filename,
                                       std::string& content)
{
    std::string content_type = request.getHeader("Content-Type");

    if (content_type.find("multipart/form-data") != std::string::npos) {
        //Logger::log(LOG_DEBUG, "process_upload_content", "Detected multipart upload");
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
        //Logger::log(LOG_DEBUG, "process_upload_content", "Detected non-multipart upload");
        filename = "upload";
        content  = request.getBody(); // raw bytes already
    }
}

// Generates a safe filename for uploads, appending timestamp.
std::string WebServer::make_upload_filename(const std::string& filename) {
    std::string safe = sanitize_filename(filename.empty() ? "upload" : filename);

    std::string base, ext;
    split_basename_ext(safe, base, ext);

    // Insert timestamp before the extension
    return base + "_" + timestamp() + ext;
}

// Writes uploaded file content to disk at specified path.
bool WebServer::write_upload_file(const std::string& full_path, const std::string& content) {
	//Opens a file stream for writing at full_path.
	/*Uses binary mode (std::ios::binary) to ensure the file is written exactly as received 
	(important for non-text files).*/
    std::ofstream out(full_path.c_str(), std::ios::binary);
    if (!out)
        return false;
    out << content;
    out.close();
    return true;
}

// Returns current timestamp as string for filenames.
std::string WebServer::timestamp() {
    time_t now = time(NULL);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}
