#include "Response.hpp"
#include "Response.hpp"
#include "../logger/Logger.hpp"
#include <unistd.h>
#include <sstream>


Response::Response() : status_code(200), status_message("OK") {
    headers["Content-Type"] = "text/html";
    headers["Connection"] = "close";
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
    return m;
}

void Response::applyConnectionHeaders(bool keepAlive) {
    setHeader("Connection", keepAlive ? "keep-alive" : "close");
    if (keepAlive) {
        setHeader("Keep-Alive", "timeout=5, max=100");
    }
}
