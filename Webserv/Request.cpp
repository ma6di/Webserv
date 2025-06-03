#include "Request.hpp"

Request::Request(const std::string& raw_data) {
    parseRequest(raw_data);
}

void    Request::parseRequest(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;

    //first line = request
    std::getline(stream, line);
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    std::istringstream req_line(line);
    req_line >> method >> path >> version;

    //parse headers
    while (std::getline(stream, line) && line != "\r") {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            size_t start = value.find_first_not_of(" ");
            if (start != std::string::npos)
                value = value.substr(start);
            headers[key] = value;
        }
    }

    //read body
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    body = body_stream.str();
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
    else
        return ("");
}

std::string Request::getBody() const {
    return body;
}

void Request::debugPrint() const {
    std::cout << "=== Received Request ===\n";
    std::cout << "Method: " << method << "\n";
    std::cout << "Path: " << path << "\n";
    std::cout << "Version: " << version << "\n";

    std::cout << "=== Headers ===\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::cout << it->first << ": " << it->second << "\n";
    }
}
