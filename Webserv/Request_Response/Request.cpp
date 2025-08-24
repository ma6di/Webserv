#include "Request.hpp"
#include "utils.hpp"
#include "Logger.hpp"
#include "WebServer.hpp"
#include <sstream>
#include <iterator>
#include <iomanip>


// Constructor: Parses the raw HTTP request data
Request::Request(const std::string& raw_data) : content_length(0) {
    parseRequest(raw_data);
}


// Getters for HTTP method, path, and version
std::string Request::getMethod() const { return method; }
std::string Request::getPath() const { return path; }
std::string Request::getVersion() const { return version; }


// Returns the value of a header by key, or empty string if not found
std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end()) return it->second;
    return "";
}


// Returns the request body
std::string Request::getBody() const { return body; }


// Parses the raw HTTP request string into method, path, version, headers, and body
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
        
        // Validate HTTP version format and support
        if (!isValidHttpVersionFormat(version))
            throw std::runtime_error("Invalid HTTP version");
        if (version.compare(0, 8, "HTTP/1.1") != 0)
            throw std::runtime_error("HTTP Version Not Supported");

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

            // Trim whitespace from key and value
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

        // Handle chunked transfer encoding and body extraction
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



// Sets the request body
void Request::setBody(const std::string& newBody) { body = newBody; }

// Returns the Content-Length value
int Request::getContentLength() const { return content_length; }


// Returns true if Transfer-Encoding is chunked
bool Request::isChunked() const {
    std::string te = getHeader("Transfer-Encoding");
    return !te.empty() && te == "chunked";
}


// Checks if the HTTP version string is valid (format: HTTP/x.y)
bool Request::isValidHttpVersionFormat(const std::string& version) const {
    // Check basic format: "HTTP/x.y"
    if (version.length() < 8)  // minimum "HTTP/x.y" is 8 chars
        return false;
    
    if (version.substr(0, 5) != "HTTP/")
        return false;
    
    if (version.length() < 8 || version[6] != '.')
        return false;
    
    // Check if major and minor versions are digits
    char major = version[5];
    char minor = version[7];
    
    if (!std::isdigit(major) || !std::isdigit(minor))
        return false;
    
    // Check for extra characters after x.y
    if (version.length() > 8)
        return false;
    
    return true;
}


// Returns true if the Expect header is "100-continue" (case-insensitive)
bool Request::hasExpectContinue() const {
    std::string expect = getHeader("Expect");
    // Convert to lowercase for case-insensitive comparison
    for (size_t i = 0; i < expect.length(); ++i) {
        if (expect[i] >= 'A' && expect[i] <= 'Z') {
            expect[i] = expect[i] - 'A' + 'a';
        }
    }
    return expect == "100-continue";
}
