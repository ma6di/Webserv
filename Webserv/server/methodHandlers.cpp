#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include <string>

void WebServer::handle_get(const Request& request, int client_fd, size_t i) {
	std::string uri = request.getPath();
    std::string path = resolve_path(uri, "GET");
    if (!file_exists(path)) {
        send_error_response(client_fd, 404, "Not Found", i);
    } else if (access(path.c_str(), R_OK) != 0) {
        send_error_response(client_fd, 403, "Forbidden", i);
    } else {
        send_response(client_fd, uri, "GET");
    }
}

void WebServer::handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string method = request.getMethod();
    std::string script_path = resolve_script_path(uri, *loc);

    // Prepare CGI environment variables
    std::map<std::string, std::string> env;
    env["REQUEST_METHOD"] = method;

    size_t q = uri.find('?');
    std::string script_name = q == std::string::npos ? uri : uri.substr(0, q);
    std::string query_string = q == std::string::npos ? "" : uri.substr(q + 1);

    env["SCRIPT_NAME"] = script_name;
    env["QUERY_STRING"] = query_string;

    if (method == "POST") {
        std::ostringstream oss;
        oss << request.getBody().size();
        env["CONTENT_LENGTH"] = oss.str();
        env["CONTENT_TYPE"] = request.getHeader("Content-Type");
    }

    // Standard CGI variables
    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"] = "HTTP/1.1";
    env["SERVER_SOFTWARE"] = "Webserv/1.0";
    env["REDIRECT_STATUS"] = "200";

    CGIHandler handler(script_path, env, request.getBody());
    std::string cgi_output = handler.execute();

    if (cgi_output == "__CGI_TIMEOUT__") {
        send_error_response(client_fd, 504, "Gateway Timeout", i);
    } else if (cgi_output == "__CGI_MISSING_HEADER__") {
        send_error_response(client_fd, 500, "Internal Server Error", i);
    } else {
        size_t header_end = cgi_output.find("\r\n\r\n");
        if (header_end == std::string::npos)
            header_end = cgi_output.find("\n\n");
        if (header_end == std::string::npos) {
            send_error_response(client_fd, 500, "CGI Output Missing Header", i);
            cleanup_client(client_fd, i);
            return;
        }

        std::string headers = cgi_output.substr(0, header_end);
        std::string body = cgi_output.substr(header_end + ((cgi_output[header_end] == '\r') ? 4 : 2));

        if (headers.find("Content-Type:") == std::string::npos) {
            send_error_response(client_fd, 500, "CGI Output Missing Content-Type", i);
            cleanup_client(client_fd, i);
            return;
        }

        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << headers << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;

        std::string response = oss.str();
        write(client_fd, response.c_str(), response.size());
    }

    cleanup_client(client_fd, i);
}


void WebServer::handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    // Handle CGI if needed
    if (loc && is_cgi_request(*loc, request.getPath())) {
		std::cout << "🔍 Handling CGI for POST request\n";
        handle_cgi(loc, request, client_fd, i);
        return;
    }
    // Handle upload
    if (handle_upload(request, loc, client_fd, i)) {
        return;
    }
    // If not upload or CGI, you can send a default response or error
    send_error_response(client_fd, 400, "Bad POST Request", i);
}

void WebServer::handle_delete(const Request& request, int client_fd, size_t i) {
    std::string uri = request.getPath();
    std::string path = resolve_path(uri, "DELETE");
    if (!file_exists(path)) {
        send_error_response(client_fd, 404, "Not Found", i);
    } else if (access(path.c_str(), W_OK) != 0) {
        send_error_response(client_fd, 403, "Forbidden", i);
    } else if (remove(path.c_str()) == 0) {
        std::string body = "<html><body><h1>File deleted: " + uri + "</h1></body></html>";
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;
        write(client_fd, oss.str().c_str(), oss.str().size());
    } else {
        send_error_response(client_fd, 500, "Internal Server Error", i);
    }
}

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

    std::string full_filename = make_upload_filename(filename);
    std::string full_path = loc->upload_dir + "/" + full_filename;

    if (!write_upload_file(full_path, content)) {
        std::cerr << "❌ Failed to open file: " << full_path << "\n";
        send_error_response(client_fd, 500, "Failed to save upload", i);
        return true;
    }

    send_upload_success_response(client_fd, full_filename, i);
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

void WebServer::send_upload_success_response(int client_fd, const std::string& full_filename, size_t i) {
    Response res;
    res.setStatus(200, "OK");
    res.setBody("<h1>✅ File uploaded as " + full_filename + "</h1>");
    std::string raw = res.toString();
    write(client_fd, raw.c_str(), raw.size());
    close(client_fd);
    fds.erase(fds.begin() + i);
}

std::string WebServer::timestamp() {
    time_t now = time(NULL);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}
