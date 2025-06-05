#include "Response.hpp"

Response::Response() : status_code(200), status_message("OK") {
    headers["Connection"] = "close";
}

void    Response::setStatus(int code, const std::string& message) {
    status_code = code;
    status_message = message;
}

void    Response::setBody(const std::string& body_content) {
    std::ostringstream oss;
    body = body_content;
    oss << body.size();
    headers["Content-Length"] = oss.str();
}

void Response::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

std::string Response::build() const {
    std::ostringstream response;

    response << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        response << it->first << ": " << it->second << "\r\n";
    
    response << "\r\n";
    response << body;

    return (response.str());
} 