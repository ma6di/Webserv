#include "Response.hpp"
#include "../logger/Logger.hpp"
#include <unistd.h>
#include <sstream>
#include <fstream>



// Centralized status messages
static std::map<int, std::string> createStatusMessages() {
    std::map<int, std::string> messages;
    messages[100] = "Continue";
    messages[200] = "OK";
    messages[201] = "Created";
    messages[204] = "No Content";
    messages[400] = "Bad Request";
    messages[401] = "Unauthorized";
    messages[403] = "Forbidden";
    messages[404] = "Not Found";
    messages[405] = "Method Not Allowed";
    messages[408] = "Request Timeout";
    messages[411] = "Length Required";
    messages[413] = "Payload Too Large";
    messages[500] = "Internal Server Error";
    messages[501] = "Not Implemented";
    messages[502] = "Bad Gateway";
    messages[503] = "Service Unavailable";
    messages[504] = "Gateway Timeout";
    messages[505] = "HTTP Version Not Supported";
    return messages;
}

static std::map<int, std::string> status_messages = createStatusMessages();

Response::Response() : status_code(200), status_message("OK") {
    headers["Content-Type"] = "text/html";
    headers["Connection"] = "close";
}
// Get status message for a code
std::string Response::getStatusMessage(int code) {
    std::map<int, std::string>::const_iterator it = status_messages.find(code);
    if (it != status_messages.end()) return it->second;
    return "Unknown";
}

// Load body from file
bool Response::loadBodyFromFile(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    body = ss.str();
    setHeader("Content-Length", to_str(body.size()));
    return true;
}

// Helper to create error response with file fallback
Response Response::createErrorResponse(int code, const std::string& error_file_path, const std::string& fallback_body) {
    Response resp;
    resp.setStatus(code, getStatusMessage(code));
    resp.setHeader("Content-Type", "text/html");
    
    if (!error_file_path.empty() && !resp.loadBodyFromFile(error_file_path)) {
        resp.setBody(fallback_body);
    }
    
    return resp;
}

Response::Response(int code,
                   const std::string& status,
                   const std::string& b,
                   const std::map<std::string,std::string>& h)
  : status_code(code)
  , status_message(status)
  , headers(h)
  , body(b)
{
    headers["Content-Length"] = to_str(body.size());
    if (headers.find("Connection") == headers.end())
        headers["Connection"] = "close";
}


void Response::setStatus(int code, const std::string& message) {
    status_code = code;
    status_message = message;
}

void Response::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void Response::setBody(const std::string& b) {
    body = b;
    if (headers.find("Content-Length") == headers.end()) {
        std::ostringstream oss;
        oss << body.size();
        headers["Content-Length"] = oss.str();
    }
}

std::string Response::toString() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        oss << it->first << ": " << it->second << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

std::map<std::string, std::string> single_header(const std::string& k, const std::string& v) {
    std::map<std::string, std::string> m;
    m[k] = v;
    return m;
}

std::map<std::string, std::string> content_type_html() {
    std::map<std::string, std::string> m;
    m["Content-Type"] = "text/html";
    return m;
}

std::map<std::string, std::string> content_type_json() {
    std::map<std::string, std::string> m;
    m["Content-Type"] = "application/json";
    return m;
}

std::map<std::string, std::string> redirect_headers(const std::string& url) {
    std::map<std::string, std::string> m;
    m["Location"] = url;
    m["Content-Type"] = "text/html";
    m["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0";
    m["Pragma"]        = "no-cache";
    m["Expires"]       = "0";
    return m;
}

void Response::applyConnectionHeaders(bool keepAlive) {
    setHeader("Connection", keepAlive ? "keep-alive" : "close");
    if (keepAlive) {
        setHeader("Keep-Alive", "timeout=5, max=100");
    }
}
