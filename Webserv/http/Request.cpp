#include "Request.hpp"
#include <sstream>
#include <iostream>

Request::Request(const std::string& raw_data) {
    parseRequest(raw_data);
}

std::string Request::getMethod() const {
    return method;
}

std::string Request::getPath() const {
    return path;
}

std::string Request::getVersion() const {
    return version;
}

std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}

std::string Request::getBody() const {
    return body;
}

void Request::parseRequest(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;

    // Parse request line: "GET /path HTTP/1.1"
    if (!std::getline(stream, line))
        throw std::runtime_error("Empty request line");
    std::istringstream request_line(line);
    request_line >> method >> path >> version;

    // Parse headers
    while (std::getline(stream, line)) {
        if (line == "\r" || line.empty())
            break;

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim leading spaces
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos)
                value = value.substr(start);

            // Remove trailing \r
            if (!value.empty() && value[value.size() - 1] == '\r')
                value.erase(value.size() - 1);

            headers[key] = value;
        }
    }

    // Read body (if any)
    std::ostringstream body_stream;
    while (std::getline(stream, line)) {
        body_stream << line << "\n";
    }
    body = body_stream.str();
    if (!body.empty() && body[body.size() - 1] == '\n')
        body.erase(body.size() - 1);
}
