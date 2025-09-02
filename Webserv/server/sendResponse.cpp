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
    keepAlive = false;                                         // <--- bug "fixed" because false
    // Apply our new helper:
    resp.applyConnectionHeaders(keepAlive); // <----- photobook bug?
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
         << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
         << "<title>Upload Successful</title>"
         << "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css\" rel=\"stylesheet\">"
         << "</head><body class=\"bg-dark text-light d-flex flex-column justify-content-center align-items-center\" "
         << "style=\"font-family:sans-serif;min-height:100vh;\">"

         << "<h1 class=\"mb-3\">✅ File uploaded successfully!</h1>"
         << "<p class=\"mb-4\">Saved as: <code>" << full_filename << "</code></p>"

         << "<div class=\"d-flex gap-3\">"
         << "<a href=\"/\" class=\"btn btn-outline-light\">Home</a>"
         << "<a href=\"/methods.html#demos\" class=\"btn btn-success\">Go back</a>"
         << "</div>"

         << "<script src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/js/bootstrap.bundle.min.js\"></script>"
         << "</body></html>";

    Logger::log(LOG_INFO, "send_upload_success_response", "Upload successful: " + full_filename);

    // Add headers: Content-Type + Location
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "text/html; charset=utf-8";
    headers["Location"] = full_filename; // ideally relative URL, not filesystem path

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
    //Logger::log(LOG_DEBUG, "resolve_error_page_path", "Resolved error path: " + full_path);
    return full_path;
}

void WebServer::send_error_response(int client_fd,
                                    int code,
                                    const std::string &msg,
                                    size_t i)
{
    (void)i;
    (void)msg;

    // Ensure the connection still exists
    std::map<int, Connection>::iterator it = conns_.find(client_fd);
    if (it == conns_.end())
        return;

    const std::string *err_page = config_->getErrorPage(code);
    std::string status_msg = Response::getStatusMessage(code);

    Response resp;
    resp.setStatus(code, status_msg);
    resp.setHeader("Content-Type", "text/html");

    bool loaded = false;
    if (err_page && !err_page->empty())
    {
        std::string resolved_path = resolve_error_page_path(*err_page);
        /*Logger::log(LOG_DEBUG, "send_error_response",
                    "Trying custom error page: " + resolved_path);*/
        loaded = resp.loadBodyFromFile(resolved_path);
        if (!loaded)
            Logger::log(LOG_ERROR, "send_error_response",
                        "Custom error page not found or unreadable: " + resolved_path);
    }

    if (!loaded)
    {
        std::ostringstream oss;
        oss << "<!DOCTYPE html><html><head><title>"
            << code << " " << status_msg
            << "</title></head><body><h1>"
            << code << " " << status_msg
            << "</h1><p>The server could not fulfill your request.</p></body></html>";
        resp.setBody(oss.str());
    }

    // Decide connection policy for errors:
    // - Keep open only for informational (1xx) or 204; otherwise close after write.
    bool closeAfter = !(code < 200 || code == 204);

    // Mark connection state
    it->second.shouldCloseAfterWrite = closeAfter;

    // Set proper Connection header
    resp.applyConnectionHeaders(!closeAfter);  // keepAlive = !closeAfter

    // Serialize and enqueue; DO NOT flush or close here
    std::string raw = resp.toString();
    queueResponse(client_fd, raw);

    // No flushPendingWrites() here — POLLOUT will handle it in the main poll loop.
}


/*void WebServer::send_continue_response(int client_fd)
{
    Response resp;
    resp.setStatus(100, Response::getStatusMessage(100));
    resp.setBody("");

    std::string response = resp.toString();
    
    ssize_t n = send(client_fd, response.c_str(), response.length(), MSG_NOSIGNAL);
    if (n > 0) {
        Logger::log(LOG_INFO, "send_continue_response",
                    "Sent 100 Continue (" + to_str(n) + " bytes) to fd=" + to_str(client_fd));
        return;
    }
    if (n == 0) {
        // peer closed
        cleanup_client(client_fd, 0);
        return;
    }
    if (n < 0) {
        Connection &conn = conns_[client_fd];
        conn.writeBuf.append(response);
        Logger::log(LOG_DEBUG, "send_continue_response",
                    "Deferring 100 Continue; queued for POLLOUT on fd=" + to_str(client_fd));
        return;
    }   
}*/

void WebServer::send_continue_response(int client_fd) {
    Response resp; resp.setStatus(100, Response::getStatusMessage(100)); resp.setBody("");
    std::string response = resp.toString();
    Connection &conn = conns_[client_fd];
    conn.writeBuf.append(response);
    // do NOT send now; POLLOUT will flush
}

