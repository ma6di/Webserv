#include "Request.hpp"


Request::Request(const std::string& raw_data) : content_length(0) { // <-- initialize
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

//Standard accessors for the HTTP request line components (GET, /index.html, HTTP/1.1)
std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}
//Fetches headers like "Content-Type" or "Host" (case-sensitive). Returns empty string if not found.
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

    // Check for malformed request line
    if (method.empty() || path.empty() || version.empty())
        throw std::runtime_error("Malformed request line");

    // Optionally, check for valid HTTP version
    if (version.compare(0, 5, "HTTP/") != 0)
        throw std::runtime_error("Missing or invalid HTTP version");

    // Parse headers
    while (std::getline(stream, line)) {
        if (line == "\r" || line.empty())
            break;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            throw std::runtime_error("Malformed header line (missing colon)");

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos)
            value = value.substr(start);

        if (!value.empty() && value[value.size() - 1] == '\r')
            value.erase(value.size() - 1);

        headers[key] = value;

        // Parse Content-Length if present
        if (key == "Content-Length") {
            std::istringstream iss(value);
            iss >> content_length;
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

void Request::setBody(const std::string& newBody) {
    this->body = newBody;
}

int Request::getContentLength() const {
    return content_length;
}

bool Request::isChunked() const {
    std::string te = getHeader("Transfer-Encoding");
    return !te.empty() && te == "chunked";
}
