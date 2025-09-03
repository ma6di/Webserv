#include "Request.hpp"
#include "Response.hpp"
#include "utils.hpp"
#include "Logger.hpp"
#include "WebServer.hpp"
#include <sstream>
#include <iterator>
#include <iomanip>


// Constructor: Parses the raw HTTP request data
Request::Request(const std::string& raw_data) : content_length(0) {
    // Safety check and logging for raw_data
        std::ostringstream oss;
        oss << "raw_data size: " << raw_data.size();
        Logger::log(LOG_INFO, "Request::Request", oss.str());
    if (raw_data.size() > 100*1024*1024) {
        Logger::log(LOG_ERROR, "Request::Request", "raw_data too large, possible buffer corruption");
        throw std::runtime_error("raw_data too large");
    }
    // Optionally log first 200 bytes for debugging
    Logger::log(LOG_INFO, "Request::Request", std::string("raw_data preview: ") + raw_data.substr(0, std::min((size_t)200, raw_data.size())));
    parseRequest(raw_data);
}


// Getters for HTTP method, path, and version
std::string Request::getMethod() const { return method; }
std::string Request::getPath() const { return path; }
std::string Request::getVersion() const { return version; }


// Returns the value of a header by key, or empty string if not found
std::string Request::getHeader(const std::string& key) const {
    // Case-insensitive header lookup
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        if (strcasecmp(it->first.c_str(), key.c_str()) == 0) {
            return it->second;
        }
    }
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
        int cl_found = 0;
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

            // Parse Content-Length if present (case-insensitive)
            if (strcasecmp(key.c_str(), "Content-Length") == 0) {
                cl_found++;
                long cl_val = -1;
                std::istringstream iss(value);
                if (!(iss >> cl_val) || cl_val < 0 || cl_val > 100L*1024L*1024L) // 100MB limit
                    throw std::runtime_error("Invalid or too large Content-Length value");
                content_length = static_cast<int>(cl_val);
            }
        }
        if (cl_found > 1)
            throw std::runtime_error("Multiple Content-Length headers");

        // Debug: print all parsed headers
        Logger::log(LOG_INFO, "Request::parseRequest", "\033[1;33m[Request] Parsed headers:\033[0m");
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            std::ostringstream oss;
            oss << "  '" << it->first << "': '" << it->second << "'";
            Logger::log(LOG_INFO, "Request::parseRequest", std::string("\033[1;33m") + oss.str() + "\033[0m");
        }

        // --- Read body ---
        std::string raw_body;
        raw_body.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

        // Body assignment: chunked already validated by buffer logic
        if (isChunked()) {
            body = raw_body;
        } else if (strcasecmp(getHeader("Content-Length").c_str(), "") != 0) {
            // Content-Length header is present (any value, including zero)
            if (content_length < 0 || content_length > 100L*1024L*1024L)
                throw std::runtime_error("Invalid or too large Content-Length value");
            if (static_cast<long>(raw_body.size()) < content_length)
                throw std::runtime_error("Incomplete body");
            body = raw_body.substr(0, content_length);
        } else {
            body = raw_body;
        }

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
    if (te.empty()) return false;

    // Split by comma and trim spaces
    std::vector<std::string> encodings;
    std::istringstream iss(te);
    std::string part;
    while (std::getline(iss, part, ',')) {
        part.erase(0, part.find_first_not_of(" \t"));
        part.erase(part.find_last_not_of(" \t") + 1);
        if (!part.empty()) encodings.push_back(part);
    }

    if (encodings.size() > 1) {
        throw std::runtime_error("501: Multiple Transfer-Encoding values: " + te);
    }
    if (encodings.size() == 1) {
        std::string val = encodings[0];
        // Lowercase for comparison
        for (size_t i = 0; i < val.size(); ++i) val[i] = std::tolower(val[i]);
        if (val != "chunked") {
            throw std::runtime_error("501: Unsupported Transfer-Encoding: " + te);
        }
        return true;
    }
    return false;
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
