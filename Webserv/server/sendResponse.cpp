#include "WebServer.hpp"

// Send a file as a response (with correct Content-Type)
void WebServer::send_file_response(int client_fd, const std::string &path, size_t i)
{
    std::string body = read_file(path);
    if (body.empty()) {
        Logger::log(LOG_ERROR, "send_file_response", "File not found or empty: " + path);
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }
    Logger::log(LOG_INFO, "send_file_response", "Sending file: " + path);
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = get_mime_type(path);
    send_ok_response(client_fd, body, headers, i);
}

// Send a redirect response
void WebServer::send_redirect_response(int client_fd, int code, const std::string &location, size_t i)
{
    (void)i;
    std::ostringstream body;
    body << "<html><head><title>" << code << " Redirect</title></head><body>"
         << "<h1>" << code << " Redirect</h1>"
         << "<p>Redirecting to <a href=\"" << location << "\">" << location << "</a></p>"
         << "</body></html>";
    Logger::log(LOG_INFO, "send_redirect_response", "Redirecting to: " + location + " (code " + to_str(code) + ")");
    
    Response resp;
    resp.setStatus(code, Response::getStatusMessage(code));
    resp.setHeader("Location", location);
    resp.setHeader("Content-Type", "text/html");
    resp.setHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    resp.setHeader("Pragma", "no-cache");
    resp.setHeader("Expires", "0");
    resp.setBody(body.str());

    bool keepAlive = !conns_[client_fd].shouldCloseAfterWrite;
    resp.applyConnectionHeaders(keepAlive);
    queueResponse(client_fd, resp.toString());
}

void WebServer::send_ok_response(int client_fd, const std::string &body, const std::map<std::string, std::string> &headers, size_t i)
{
    (void)i;
    Logger::log(LOG_INFO, "send_ok_response", "Sending 200 OK response.");
    Response resp(200, "OK", body, headers);
    bool keepAlive = !conns_[client_fd].shouldCloseAfterWrite; // <----- photobook bug?
    keepAlive = false; // <--- bug "fixed" because false
    // Apply our new helper:
    resp.applyConnectionHeaders(keepAlive);  // <----- photobook bug?
    std::string raw = resp.toString();
    // 2) Enqueue for non-blocking write; close after fully sent
    queueResponse(client_fd, raw);
}


void WebServer::send_created_response(int client_fd, 
                                      const std::string &body, 
                                      const std::map<std::string, std::string> &headers, 
                                      size_t i)
{
    (void)i;
    Logger::log(LOG_INFO, "send_created_response", "Sending 201 Created response.");
    Response resp(201, "Created", body, headers);

    bool keepAlive = !conns_[client_fd].shouldCloseAfterWrite;

    // Apply connection headers (keep-alive/close)
    resp.applyConnectionHeaders(keepAlive);

    std::string raw = resp.toString();
    queueResponse(client_fd, raw);
}

void WebServer::send_upload_success_response(int client_fd, const std::string &full_filename, size_t i)
{
    std::ostringstream body;
    body << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
         << "<title>Upload Successful</title></head><body style=\"font-family:sans-serif;text-align:center;margin-top:50px;\">"
         << "<h1>✅ File uploaded successfully!</h1>"
         << "<p>Saved as: <code>" << full_filename << "</code></p>"
         << "<br><br>"
         << "<a href=\"/\" style=\"margin: 0 10px;\"><button>Home</button></a>"
         << "<a href=\"/about\" style=\"margin: 0 10px;\"><button>About</button></a>"
         << "<a href=\"/upload\" style=\"margin: 0 10px;\"><button>Upload Another</button></a>"
         << "</body></html>";

    Logger::log(LOG_INFO, "send_upload_success_response", "Upload successful: " + full_filename);

    // Add headers: Content-Type + Location
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "text/html; charset=utf-8";
    headers["Location"] = full_filename;   // ideally relative URL, not filesystem path

    send_created_response(client_fd, body.str(), headers, i);
}

void WebServer::send_no_content_response(int client_fd, size_t i)
{
    (void)i;
    Logger::log(LOG_INFO, "send_no_content_response", "Sending 204 No Content response.");

    // Empty headers (Content-Length/Content-Type not allowed for 204)
    std::map<std::string, std::string> headers;

    // Build response object
    Response resp(204, "No Content", "", headers);

    bool keepAlive = conns_[client_fd].shouldCloseAfterWrite;
    resp.applyConnectionHeaders(keepAlive);

    // Let Response::toString() handle proper formatting
    std::string raw = resp.toString();

    queueResponse(client_fd, raw);
}


static std::string resolve_error_page_path(const std::string &err_uri)
{
    std::string fallback_root = "./www";
    std::string cleaned_uri = err_uri;

    if (!cleaned_uri.empty() && cleaned_uri[0] == '/')
        cleaned_uri = cleaned_uri.substr(1);

    std::string full_path = fallback_root + "/" + cleaned_uri;
    Logger::log(LOG_DEBUG, "resolve_error_page_path", "Resolved error path: " + full_path);
    return full_path;
}

void WebServer::send_error_response(int client_fd,
                                    int code,
                                    const std::string &msg,
                                    size_t i)
{
    (void)i;
    (void)msg; // Suppress unused parameter warning
    const std::string *err_page = config_->getErrorPage(code);
    std::string status_msg = Response::getStatusMessage(code);
    Response resp;
    resp.setStatus(code, status_msg);
    resp.setHeader("Content-Type", "text/html");
    bool loaded = false;
    if (err_page && !err_page->empty()) {
        std::string resolved_path = resolve_error_page_path(*err_page);
        Logger::log(LOG_DEBUG, "send_error_response", "Trying custom error page: " + resolved_path);
        loaded = resp.loadBodyFromFile(resolved_path);
        if (!loaded) {
            Logger::log(LOG_ERROR, "send_error_response", "Custom error page not found or not readable: " + resolved_path);
        }
    }
    if (!loaded) {
        std::ostringstream oss;
        oss << "<!DOCTYPE html><html><head><title>" << code << " " << status_msg
            << "</title></head><body><h1>" << code << " " << status_msg
            << "</h1><p>The server could not fulfill your request.</p></body></html>";
        resp.setBody(oss.str());
    }
    // For error responses, close connection after sending unless informational (1xx) or 204
    if (code < 200 || code == 204) {
        conns_[client_fd].shouldCloseAfterWrite = false;
    } else {
        conns_[client_fd].shouldCloseAfterWrite = true;
    }
    bool keepAlive = !conns_[client_fd].shouldCloseAfterWrite;
    resp.applyConnectionHeaders(keepAlive);
    std::string raw = resp.toString();
    queueResponse(client_fd, raw);
    // Always flush writes for error responses
    flushPendingWrites(client_fd);
}

// ...existing code...

// void WebServer::send_length_required_response(int client_fd, const std::string &details)
// {
//     Response resp;
//     resp.setStatus(411, Response::getStatusMessage(411));
//     resp.setHeader("Content-Type", "text/html");

//     // Try to load custom error page first
//     if (!resp.loadBodyFromFile("./www/error_pages/411.html")) {
//         // Fallback to default error page
//         std::ostringstream oss;
//         oss << "<!DOCTYPE html><html><head><title>411 Length Required</title></head>"
//             << "<body><h1>411 Length Required</h1>";

//         if (!details.empty()) {
//             oss << "<p>" << details << "</p>";
//         } else {
//             oss << "<p>Length Required</p>";
//         }
//         oss << "</body></html>";
//         resp.setBody(oss.str());
//     }

//     bool keepAlive = !conns_[client_fd].shouldCloseAfterWrite;
//     resp.applyConnectionHeaders(keepAlive);
//     queueResponse(client_fd, resp.toString());
// }

// void WebServer::send_request_timeout_response(int client_fd, size_t i) {
//     (void)i;
//     Logger::log(LOG_INFO, "send_request_timeout_response", 
//                 "Sending 408 Request Timeout response to fd=" + to_str(client_fd));

//     Response resp;
//     resp.setStatus(408, Response::getStatusMessage(408));
//     resp.setHeader("Content-Type", "text/html; charset=utf-8");
//     resp.setHeader("Connection", "close");

//     // Try to load custom error page first
//     if (!resp.loadBodyFromFile("./www/error_pages/408.html")) {
//         // Fallback to default error page
//         std::string body = "<!DOCTYPE html><html><head><title>408 Request Timeout</title></head>"
//                           "<body><h1>408 Request Timeout</h1>"
//                           "<p>Your request took too long to complete.</p>"
//                           "<p><a href=\"/\">Go back to homepage</a></p>"
//                           "</body></html>";
//         resp.setBody(body);
//     }

//     // Send immediately without queuing (for timeout situations)
//     std::string response = resp.toString();
//     ssize_t n = ::write(client_fd, response.data(), response.size());
//     if (n < 0) {
//         Logger::log(LOG_ERROR, "send_request_timeout_response", 
//                    "Failed to send 408 response to fd=" + to_str(client_fd) + 
//                    " (errno=" + to_str(errno) + ")");
//     } else {
//         Logger::log(LOG_INFO, "send_request_timeout_response", 
//                    "Successfully sent 408 response (" + to_str(n) + " bytes) to fd=" + to_str(client_fd));
//     }
// }

void WebServer::send_continue_response(int client_fd)
{
    Response resp;
    resp.setStatus(100, Response::getStatusMessage(100));
    // 100 Continue should have no body and minimal headers
    resp.setBody("");
    
    std::string response = resp.toString();
    ssize_t n = send(client_fd, response.c_str(), response.length(), MSG_NOSIGNAL);
    if (n == -1) {
        Logger::log(LOG_ERROR, "send_continue_response", 
                   "Failed to send 100 Continue response to fd=" + to_str(client_fd) + 
                   " (errno=" + to_str(errno) + ")");
    } else {
        Logger::log(LOG_INFO, "send_continue_response", 
                   "Successfully sent 100 Continue response (" + to_str(n) + " bytes) to fd=" + to_str(client_fd));
    }
}
