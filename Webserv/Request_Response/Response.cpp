#include "Response.hpp"

//Default response: 200 OK
//Content-Type: tells browser it's HTML
//Connection: close: connection won't be reused
Response::Response() : status_code(200), status_message("OK") {
    headers["Content-Type"] = "text/html";
    headers["Connection"] = "close";
}

// Response::Response(int code, const std::string& message)
//     : status_code(code), status_message(message) {
//     headers["Content-Type"] = "text/html";
//     headers["Connection"] = "close";
// }

/*Response::Response(int client_fd, int code, const std::string& message, const std::string& body,
                   const std::map<std::string, std::string>& extra_headers)
    : status_code(code), status_message(message), body(body)
{
    headers["Content-Type"] = "text/html";
    headers["Connection"] = "close";
    std::ostringstream oss;
    oss << body.size();
    headers["Content-Length"] = oss.str();

    // Add/override any extra headers
    for (std::map<std::string, std::string>::const_iterator it = extra_headers.begin(); it != extra_headers.end(); ++it) {
        headers[it->first] = it->second;
    }

    // Write response immediately
    std::string raw = toString();
    write(client_fd, raw.c_str(), raw.size());
}*/

#include "Response.hpp"
#include "../logger/Logger.hpp"
#include <unistd.h>
#include <sstream>

Response::Response(int client_fd,
                   int code,
                   const std::string& status,
                   const std::string& body,
                   const std::map<std::string,std::string>& headers)
{
    // 1) Build the HTTP response
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status << "\r\n";
    for (std::map<std::string,std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
    {
        oss << it->first << ": " << it->second << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

    std::string raw = oss.str();

    // 2) Log what we’re about to send
    Logger::log(LOG_DEBUG, "Response",
                "=== BEGIN RAW RESPONSE ===\n" +
                raw +
                "\n===  END RAW RESPONSE  ===");

    // 3) Write to the socket
    ssize_t n = write(client_fd, raw.c_str(), raw.size());
    Logger::log(LOG_DEBUG, "Response",
                "Wrote " + to_str(n) + " bytes to FD=" + to_str(client_fd));

    // 4) Shutdown write side so the client sees EOF
    shutdown(client_fd, SHUT_WR);
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

// Utility: Content-Type: text/html
std::map<std::string, std::string> content_type_html() {
    std::map<std::string, std::string> m;
    m["Content-Type"] = "text/html";
    return m;
}

// Utility: Content-Type: application/json
std::map<std::string, std::string> content_type_json() {
    std::map<std::string, std::string> m;
    m["Content-Type"] = "application/json";
    return m;
}

// Utility: Redirect headers (Location + Content-Type)
std::map<std::string, std::string> redirect_headers(const std::string& url) {
    std::map<std::string, std::string> m;
    m["Location"] = url;
    m["Content-Type"] = "text/html";
    return m;
}

