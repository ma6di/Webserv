#include "WebServer.hpp"

std::string WebServer::resolve_path(const std::string &raw_path,
                                    const std::string &method,
                                    const LocationConfig *loc)
{
    Logger::log(LOG_DEBUG, "resolve_path",
                "raw_path = \"" + raw_path + "\", method = " + method);

    // 1) Pick the base filesystem root:
    //    - If the location block set a root, use it.
    //    - Otherwise fall back to the server-level root from config_.
    const std::string &serverRoot = config_->getRoot();
    std::string base = (loc && !loc->root.empty())
                           ? loc->root
                           : serverRoot;

    // 2) Strip off the location prefix from the URI, if any
    std::string rel = raw_path;
    if (loc && loc->path != "/" && rel.find(loc->path) == 0)
    {
        rel = rel.substr(loc->path.length());
    }
    if (!rel.empty() && rel[0] == '/')
        rel = rel.substr(1);

    // 3) Build the candidate filesystem path
    std::string candidate = base;
    if (!rel.empty())
        candidate += "/" + rel;

    Logger::log(LOG_DEBUG, "resolve_path", "Candidate path: " + candidate);

    // 4) If it’s a directory, hand it off to handle_directory_request()
    if (is_directory(candidate))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "Directory detected, returning: " + candidate);
        return candidate;
    }

    // 5) If it’s an existing file, return it
    if (file_exists(candidate))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "File exists, returning: " + candidate);
        return candidate;
    }

    // 6) Try adding “.html”
    std::string html_fallback = candidate + ".html";
    if (file_exists(html_fallback))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "HTML fallback, returning: " + html_fallback);
        return html_fallback;
    }

    // 7) Nothing matched: return candidate so your 404 logic fires
    Logger::log(LOG_DEBUG, "resolve_path",
                "Nothing found, returning: " + candidate);
    return candidate;
}

std::string extract_filename(const std::string &header)
{
    size_t pos = header.find("filename=");
    if (pos == std::string::npos)
        return "upload";

    size_t start = header.find('"', pos);
    size_t end = header.find('"', start + 1);
    if (start == std::string::npos || end == std::string::npos)
        return "upload";

    return header.substr(start + 1, end - start - 1);
}

// Reads the entire contents of a file into a string
std::string WebServer::read_file(const std::string &path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
    {
        Logger::log(LOG_ERROR, "read_file", "Failed to open file: " + path);
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WebServer::read_and_append_client_data(int client_fd, size_t i)
{
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0)
    {
        cleanup_client(client_fd, i); // EOF
        return false;
    }
    if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;             // not fatal
        cleanup_client(client_fd, i); // real error
        return false;
    }
    conns_[client_fd].readBuf.append(buffer, static_cast<size_t>(bytes_read));

    if (conns_[client_fd].readBuf.size() > config_->getMaxBodySize())
    {
        Logger::log(LOG_ERROR, "read_and_append_client_data", "Payload Too Large for FD=" + to_str(client_fd));
        send_error_response(client_fd, 413, "Payload Too Large", i);
        usleep(100000);
        return false;
    }

    return true;
}

size_t WebServer::find_header_end(const std::string &request_data)
{
    return request_data.find("\r\n\r\n");
}

int WebServer::parse_content_length(const std::string &headers)
{
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.find("Content-Length:") != std::string::npos)
        {
            std::istringstream linestream(line);
            std::string key;
            int content_length = 0;
            linestream >> key >> content_length;
            return content_length;
        }
    }
    return 0;
}

// --- Helper: Check if full body is received ---
bool WebServer::is_full_body_received(const Request &request, const std::string &request_data, size_t header_end)
{
    bool is_chunked = request.isChunked();
    int content_length = request.getContentLength();
    size_t body_start = header_end + 4;
    size_t body_length_received = request_data.size() - body_start;

    if (!is_chunked && content_length > 0 && body_length_received < static_cast<size_t>(content_length))
    {
        return false;
    }
    if (is_chunked && request.getBody().empty())
    {
        return false;
    }
    return true;
}
