#include "Request.hpp"

Request::Request(const std::string& raw_data) {
    parseRequest(raw_data);
}

/*void    Request::parseRequest(const std::string& raw_data) {
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

    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    body = body_stream.str();
    std::cout << "[DEBUG] Parsed method: '" << method << "' path: '" << path << "' version: '" << version << "'\n";

}*/

/*void Request::parseRequest(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;

    if (!std::getline(stream, line))
        return;

    if (!line.empty() && line[line.length() - 1] == '\r')
        line.erase(line.length() - 1);

    std::istringstream req_line(line);
    req_line >> this->method >> this->path >> this->version;

    // DEBUG: Show raw request line
    std::cout << "[DEBUG] Parsed method: '" << this->method << "' path: '" << this->path << "' version: '" << this->version << "'\n";

    // Remove query string from path
    size_t pos = this->path.find('?');
    if (pos != std::string::npos)
        this->path = this->path.substr(0, pos);

    // Parse headers
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

    // Body (optional)
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    this->body = body_stream.str();
    std::cout << "[DEBUG] Parsed method: '" << method << "' path: '" << path << "' version: '" << version << "'\n";

}*/

/*void Request::parseRequest(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;

    if (!std::getline(stream, line))
        return;

    if (!line.empty() && line[line.length() - 1] == '\r')
        line.erase(line.length() - 1);

    std::istringstream req_line(line);
    req_line >> this->method >> this->path >> this->version;

    // DEBUG: Show raw request line
    std::cout << "[DEBUG] Parsed method: '" << this->method << "' path: '" << this->path << "' version: '" << this->version << "'\n";

    // Remove query string from path
    size_t pos = this->path.find('?');
    if (pos != std::string::npos)
        this->path = this->path.substr(0, pos);

    // Parse headers
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

    // Body (optional)
    std::ostringstream body_stream;
    body_stream << stream.rdbuf();
    this->body = body_stream.str();
}*/

/*void Request::parseRequest(const std::string& raw) {
    std::istringstream stream(raw);
    std::string request_line;

    method.clear();
    path.clear();
    version.clear();

    if (!std::getline(stream, request_line) || request_line.empty()) {
        std::cerr << "[ERROR] Malformed request: empty first line\n";
        return;
    }

    std::istringstream line_stream(request_line);
    if (!(line_stream >> method >> path >> version)) {
        std::cerr << "[ERROR] Malformed request line: " << request_line << "\n";
        method.clear();
        path.clear();
        version.clear();
        return;
    }

    // Normalize method to uppercase if needed
    std::transform(method.begin(), method.end(), method.begin(), ::toupper);

    // You can optionally parse headers here too, if needed
}*/

void Request::parseRequest(const std::string& raw) {
    std::istringstream stream(raw);
    std::string line;

    if (!std::getline(stream, line) || line.empty()) {
        std::cerr << "[ERROR] Malformed request: empty first line\n";
        return;
    }

    std::istringstream req_line(line);
    if (!(req_line >> method >> path >> version)) {
        std::cerr << "[ERROR] Malformed request line: " << line << "\n";
        return;
    }

    std::transform(method.begin(), method.end(), method.begin(), ::toupper);

    // ✅ Restore header parsing
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

    // Optional: Parse body
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
