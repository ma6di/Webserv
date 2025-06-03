#include "Response.hpp"
#include <sstream>

Response::Response() : status_code(200), status_message("OK") {
    headers["Content-Type"] = "text/html";
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
    std::ostringstream oss;
    oss << body.size();
    headers["Content-Length"] = oss.str();
}

std::string Response::toString() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        oss << it->first << ": " << it->second << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}
