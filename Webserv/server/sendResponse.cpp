#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"

extern Config g_config;

void WebServer::send_response(int client_fd, const std::string& raw_path, const std::string& method) {
    std::string file_path = resolve_path(raw_path, method);
    std::ifstream file(file_path.c_str());
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << get_mime_type(file_path) << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string response = oss.str();
    write(client_fd, response.c_str(), response.size());
}

// Load a custom or default error response and send it
void WebServer::send_error_response(int client_fd, int code, const std::string& reason, size_t i) {
    std::string body;
    std::string custom_path = get_custom_error_page_path(code); // Implement this
    if (!custom_path.empty() && file_exists(custom_path)) {
        body = read_file(custom_path); // Implement this
    } else {
        body = get_default_error_page(code);
    }
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << reason << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    write(client_fd, oss.str().c_str(), oss.str().size());
    close(client_fd);
    fds.erase(fds.begin() + i);
}

// Returns the path to a custom error page for a given code, or empty string if not set
std::string WebServer::get_custom_error_page_path(int code) {
    // Example: check config for custom error page
    std::map<int, std::string>::const_iterator it = g_config.getErrorPages().find(code);
    if (it != g_config.getErrorPages().end())
        return it->second;
    return "";
}

// Returns a default HTML error page for the given code
std::string WebServer::get_default_error_page(int code) {
    switch (code) {
        case 400:
            return "<html><head><title>400 Bad Request</title></head>"
                   "<body><h1>400 Bad Request</h1><p>Your browser sent a request that this server could not understand.</p></body></html>";
        case 401:
            return "<html><head><title>401 Unauthorized</title></head>"
                   "<body><h1>401 Unauthorized</h1><p>Authentication is required to access this resource.</p></body></html>";
        case 403:
            return "<html><head><title>403 Forbidden</title></head>"
                   "<body><h1>403 Forbidden</h1><p>You don't have permission to access this resource.</p></body></html>";
        case 404:
            return "<html><head><title>404 Not Found</title></head>"
                   "<body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
        case 405:
            return "<html><head><title>405 Method Not Allowed</title></head>"
                   "<body><h1>405 Method Not Allowed</h1><p>The method is not allowed for the requested URL.</p></body></html>";
        case 408:
            return "<html><head><title>408 Request Timeout</title></head>"
                   "<body><h1>408 Request Timeout</h1><p>The server timed out waiting for the request.</p></body></html>";
        case 413:
            return "<html><head><title>413 Payload Too Large</title></head>"
                   "<body><h1>413 Payload Too Large</h1><p>The request is too large for this server to process.</p></body></html>";
        case 500:
            return "<html><head><title>500 Internal Server Error</title></head>"
                   "<body><h1>500 Internal Server Error</h1><p>The server encountered an internal error.</p></body></html>";
        case 501:
            return "<html><head><title>501 Not Implemented</title></head>"
                   "<body><h1>501 Not Implemented</h1><p>The server does not support the functionality required to fulfill the request.</p></body></html>";
        case 502:
            return "<html><head><title>502 Bad Gateway</title></head>"
                   "<body><h1>502 Bad Gateway</h1><p>The server received an invalid response from the upstream server.</p></body></html>";
        case 503:
            return "<html><head><title>503 Service Unavailable</title></head>"
                   "<body><h1>503 Service Unavailable</h1><p>The server is currently unable to handle the request due to temporary overload or maintenance.</p></body></html>";
        case 504:
            return "<html><head><title>504 Gateway Timeout</title></head>"
                   "<body><h1>504 Gateway Timeout</h1><p>The server did not receive a timely response from the upstream server.</p></body></html>";
        default:
            return "<html><head><title>Error</title></head>"
                   "<body><h1>An error occurred</h1></body></html>";
    }
}
