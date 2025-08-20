#include "Request.hpp"
#include "utils.hpp"
#include "Logger.hpp"
#include "WebServer.hpp"
#include <sstream>
#include <iterator>
#include <iomanip>

Request::Request(const std::string& raw_data) : content_length(0) {
    parseRequest(raw_data);
}

std::string Request::getMethod() const { return method; }
std::string Request::getPath() const { return path; }
std::string Request::getVersion() const { return version; }

std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end()) return it->second;
    return "";
}

std::string Request::getBody() const { return body; }

void Request::parseRequest(const std::string& raw_data) {
    std::istringstream stream(raw_data);
    std::string line;

    // Reset previous state
    method.clear();
    path.clear();
    version.clear();
    headers.clear();
    content_length = 0;
    body.clear();

    try {
        // --- Parse request line ---
        if (!std::getline(stream, line))
            throw std::runtime_error("Empty request line");
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        std::istringstream request_line(line);
        request_line >> method >> path >> version;
        if (method.empty() || path.empty() || version.empty())
            throw std::runtime_error("Malformed request line");
        if (version.compare(0, 8, "HTTP/1.1") != 0)
            throw std::runtime_error("Invalid HTTP version");

        // --- Parse headers ---
        while (std::getline(stream, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.empty())
                break; // end of headers

            size_t colon = line.find(':');
            if (colon == std::string::npos)
                throw std::runtime_error("Malformed header line (missing colon)");

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key.empty())
                throw std::runtime_error("Malformed header line (empty key)");

            headers[key] = value;

            // Parse Content-Length if present
            if (key == "Content-Length") {
                std::istringstream iss(value);
                if (!(iss >> content_length))
                    throw std::runtime_error("Invalid Content-Length value");
            }
        }

        // --- Read body ---
        std::string raw_body;
        raw_body.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

        if (isChunked()) {
            body = decode_chunked_body(raw_body);
        } else if (content_length > 0) {
            if (static_cast<int>(raw_body.size()) < content_length)
                throw std::runtime_error("Incomplete body");
            body = raw_body.substr(0, content_length);
        } else {
            body = raw_body;
        }
        // All chunked decoding is handled here only

    } catch (const std::runtime_error& e) {
        Logger::log(LOG_ERROR, "Request::parseRequest", e.what());
        throw; // propagate exception to server to send 400 response
    }
}


void Request::setBody(const std::string& newBody) { body = newBody; }
int Request::getContentLength() const { return content_length; }

bool Request::isChunked() const {
    std::string te = getHeader("Transfer-Encoding");
    return !te.empty() && te == "chunked";
}

