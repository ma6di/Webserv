#include "utils.hpp"

bool file_exists(const std::string& path) {
    struct stat buffer;
    bool exists = (stat(path.c_str(), &buffer) == 0);
    Logger::log(LOG_DEBUG, "file_exists", "Checked: " + path + " exists=" + (exists ? "true" : "false"));
    return exists;
}

std::string get_mime_type(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        Logger::log(LOG_DEBUG, "get_mime_type", "No extension for: " + path);
        return "application/octet-stream";
    }

    std::string ext = path.substr(dot);
    std::string mime;

    if (ext == ".html")
        mime = "text/html";
    else if (ext == ".css")
        mime = "text/css";
    else if (ext == ".js")
        mime = "application/javascript";
    else if (ext == ".txt")
        mime = "text/plain";
    else if (ext == ".jpg" || ext == ".jpeg")
        mime = "image/jpeg";
    else if (ext == ".png")
        mime = "image/png";
    else if (ext == ".gif")
        mime = "image/gif";
    else
        mime = "application/octet-stream";

    Logger::log(LOG_DEBUG, "get_mime_type", "Path: " + path + " -> " + mime);
    return mime;
}

/*const LocationConfig* match_location(const std::vector<LocationConfig>& locations, const std::string& path) {
    const LocationConfig* bestMatch = NULL;
    size_t bestLength = 0;

    for (size_t i = 0; i < locations.size(); ++i) {
        const std::string& locPath = locations[i].path;
        if (path.find(locPath) == 0 && locPath.length() > bestLength) {
            bestLength = locPath.length();
            bestMatch = &locations[i];
        }
    }

    if (bestMatch)
        Logger::log(LOG_DEBUG, "match_location", "Matched: " + bestMatch->path + " for path: " + path);
    else
        Logger::log(LOG_DEBUG, "match_location", "No match for path: " + path);

    return bestMatch;
}*/

const LocationConfig* match_location(
    const std::vector<LocationConfig>& locations,
    const std::string& path)
{
    const LocationConfig* best = NULL;
    size_t bestLen = 0;

    for (size_t i = 0; i < locations.size(); ++i) {
        const std::string& loc = locations[i].path;

        // 1) must be prefix …
        if (path.compare(0, loc.length(), loc) != 0)
            continue;

        // 2) … and either exact match, or next char is '/'
        if (path.length() > loc.length() &&
            loc != "/" &&
            path[loc.length()] != '/')
        {
            continue;
        }

        // 3) take the longest valid prefix
        if (loc.length() > bestLen) {
            bestLen = loc.length();
            best    = &locations[i];
        }
    }

    if (best)
        Logger::log(LOG_DEBUG,
                    "match_location",
                    "Matched: " + best->path + " for path: " + path);
    else
        Logger::log(LOG_DEBUG,
                    "match_location",
                    "No match for path: " + path);

    return best;
}


bool is_cgi_request(const LocationConfig& loc, const std::string& uri) {
    std::string cgi_uri = loc.path;  // e.g. /cgi-bin
    std::string cgi_root = loc.root; // e.g. www/cgi-bin

    if (uri.find(cgi_uri) != 0)
        return false;

    // Get the path after the location prefix
    std::string rel_uri = uri.substr(cgi_uri.length());
    if (!rel_uri.empty() && rel_uri[0] == '/')
        rel_uri = rel_uri.substr(1);

    // Only check the first segment as the script
    size_t slash = rel_uri.find('/');
    std::string script_name = (slash == std::string::npos) ? rel_uri : rel_uri.substr(0, slash);
    if (script_name.empty())
        return false;

    std::string abs_script = cgi_root;
    if (!abs_script.empty() && abs_script[abs_script.size() - 1] != '/')
        abs_script += "/";
    abs_script += script_name;

    Logger::log(LOG_DEBUG, "is_cgi_request", "abs_script: [" + abs_script + "]");
    bool valid = file_exists(abs_script) && access(abs_script.c_str(), X_OK) == 0;
    Logger::log(LOG_DEBUG, "is_cgi_request", std::string("CGI valid: ") + (valid ? "true" : "false"));
    return valid;
}

// std::string decode_chunked_body(const std::string& body) {
//     std::istringstream in(body);
//     std::string decoded, line;
//     while (std::getline(in, line)) {
//         // Remove trailing \r if present
//         if (!line.empty() && line[line.size() - 1] == '\r')
//             line.erase(line.size() - 1);
//         if (line.empty())
//             continue;
//         // Parse chunk size (hex)
//         size_t chunk_size = 0;
//         std::istringstream chunk_size_stream(line);
//         chunk_size_stream >> std::hex >> chunk_size;
//         if (chunk_size == 0)
//             break;
//         // Read chunk data
//         std::string chunk(chunk_size, '\0');
//         in.read(&chunk[0], chunk_size);
//         decoded += chunk;
//         // Read the trailing \r\n after chunk data
//         std::getline(in, line);
//     }
//     Logger::log(LOG_DEBUG, "decode_chunked_body", "Decoded chunked body, size=" + to_str(decoded.size()));
//     return decoded;
// }

std::string decode_chunked_body(const std::string& raw) {
    std::istringstream in(raw);
    std::string decoded;
    std::string line;

    while (std::getline(in, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line[line.size() - 1] == '\r') 
            line.erase(line.size() - 1);

        if (line.empty()) 
            continue;

        // Parse chunk size in hex
        size_t chunk_size = 0;
        std::istringstream iss(line);
        iss >> std::hex >> chunk_size;
        if (chunk_size == 0) 
            break;

        // Read chunk data
        std::string chunk(chunk_size, '\0');
        in.read(&chunk[0], chunk_size);
        if (static_cast<size_t>(in.gcount()) < chunk_size) {
            throw std::runtime_error("Incomplete chunked body");
        }

        decoded += chunk;

        // Consume trailing \r\n after chunk data
        if (!std::getline(in, line)) 
            break;
    }

    return decoded;
}

bool is_directory(const std::string& path) {
    struct stat statbuf;
    bool dir = stat(path.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
    Logger::log(LOG_DEBUG, "is_directory", path + " is_directory=" + (dir ? "true" : "false"));
    return dir;
}

std::string generate_directory_listing(const std::string& dir_path, const std::string& uri_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        Logger::log(LOG_ERROR, "generate_directory_listing", "Failed to open dir: " + dir_path);
        return "<html><body><h1>403 Forbidden</h1></body></html>";
    }

    std::ostringstream html;
    html << "<html><head><title>Index of " << uri_path << "</title></head><body>";
    html << "<h1>Index of " << uri_path << "</h1><ul>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;

        html << "<li><a href=\"" << uri_path;
        if (!uri_path.empty() && uri_path[uri_path.length() - 1] != '/') html << "/";
        html << name << "\">" << name << "</a></li>";
    }

    html << "</ul></body></html>";
    closedir(dir);
    Logger::log(LOG_DEBUG, "generate_directory_listing", "Generated listing for: " + dir_path);
    return html.str();
}

std::string sanitize_filename(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '/' || c == '\\') continue; 
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') {
            out += c;
        }
    }
    return out.empty() ? std::string("upload") : out;
}

void split_basename_ext(const std::string& name, std::string& base, std::string& ext) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        base = name;
        ext  = "";
    } else {
        base = name.substr(0, dot);
        ext  = name.substr(dot); 
    }
}


std::string get_boundary_from_content_type(const std::string& contentType) {
    const std::string key = "boundary=";
    size_t pos = contentType.find(key);
    if (pos == std::string::npos) return "";
    std::string b = contentType.substr(pos + key.size());
    if (!b.empty() && b[0] == '"' && b[b.size()-1] == '"')
        b = b.substr(1, b.size() - 2);
    return b;
}

bool extract_multipart_file_raw(const std::string& body,
                                       const std::string& boundary,
                                       std::string& outFilename,
                                       std::string& outContent)
{
    if (boundary.empty()) return false;

    const std::string dashBoundary = std::string("--") + boundary;
    const std::string CRLF = "\r\n";

    // Find first boundary line
    size_t pos = body.find(dashBoundary + CRLF);
    if (pos == std::string::npos) return false;
    pos += dashBoundary.size() + CRLF.size();

    while (true) {
        // Find headers end
        size_t headersEnd = body.find(CRLF + CRLF, pos);
        if (headersEnd == std::string::npos) return false;

        std::string headers = body.substr(pos, headersEnd - pos);

        // Parse filename from Content-Disposition
        std::string filename;
        size_t cd = headers.find("Content-Disposition:");
        if (cd != std::string::npos) {
            size_t fn = headers.find("filename=", cd);
            if (fn != std::string::npos) {
                size_t q1 = headers.find('"', fn);
                if (q1 != std::string::npos) {
                    size_t q2 = headers.find('"', q1 + 1);
                    if (q2 != std::string::npos) {
                        filename = headers.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
        }

        size_t contentStart = headersEnd + 4; // skip \r\n\r\n

        // Next boundary (either middle or closing)
        size_t next = body.find(CRLF + dashBoundary, contentStart);
        if (next == std::string::npos) {
            // Try without preceding CRLF (edge case)
            next = body.find(dashBoundary, contentStart);
            if (next == std::string::npos) return false;
        }

        // Content ends just before the CRLF preceding the next boundary (if present)
        size_t contentEnd = next;
        if (contentEnd >= 2 && body.substr(contentEnd - 2, 2) == CRLF)
            contentEnd -= 2;

        // If this part has a filename, we treat it as the file part
        if (!filename.empty()) {
            // strip any path components
            size_t slash = filename.find_last_of("/\\");
            if (slash != std::string::npos) filename = filename.substr(slash + 1);

            outFilename = sanitize_filename(filename);
            outContent.assign(body.data() + contentStart, body.data() + contentEnd);
            return true;
        }

        // Move to the start of the next part
        size_t afterBoundary = body.find(CRLF, next + dashBoundary.size());
        if (afterBoundary == std::string::npos) return false;
        // closing boundary ends with "--"
        if (body.compare(next, dashBoundary.size() + 2, dashBoundary + "--") == 0)
            return false; // reached end without finding a file
        pos = afterBoundary + CRLF.size();
    }
}

// 1) Check "Transfer-Encoding: chunked" in a header substring (case-insensitive)
bool has_chunked_encoding(const std::string& headers) {
    std::string h = headers; // copy
    for (size_t i = 0; i < h.size(); ++i) {
        char c = h[i];
        if (c >= 'A' && c <= 'Z') h[i] = char(c - 'A' + 'a');
    }
    return h.find("transfer-encoding:") != std::string::npos
        && h.find("chunked") != std::string::npos;
}

// 2) Find the end of a chunked body quickly (looks for the 0-chunk terminator)
size_t find_chunked_terminator(const std::string& buf, size_t body_start) {
    const std::string endMarker = "\r\n0\r\n\r\n";
    size_t pos = buf.find(endMarker, body_start);
    if (pos == std::string::npos) return std::string::npos;
    return pos + endMarker.size(); // absolute end index for the whole request
}

