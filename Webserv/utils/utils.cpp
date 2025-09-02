#include "utils.hpp"

/*
JESS: json helper functions
    wants_json: a bool that returns true or false based on if the server has to give json response or not
    json_headers: returns header in json format
*/
bool wants_json(const Request &req)
{
    // 1) custom header from your frontend
    std::string xf = req.getHeader("X-Frontend");
    if (!xf.empty() && (xf == "1" || xf == "true"))
        return true;

    // 2) Accept header
    std::string acc = req.getHeader("Accept");
    if (acc.find("application/json") != std::string::npos)
        return true;

    // 3) query flag ?json=1
    const std::string &p = req.getPath();
    std::string::size_type q = p.find('?');
    if (q != std::string::npos && p.find("json=1", q) != std::string::npos)
        return true;

    return false;
}

std::map<std::string, std::string> json_headers()
{
    std::map<std::string, std::string> h;
    h["Content-Type"] = "application/json; charset=utf-8";
    return h;
}

bool file_exists(const std::string &path)
{
    struct stat buffer;
    bool exists = (stat(path.c_str(), &buffer) == 0);
    Logger::log(LOG_DEBUG, "file_exists", "Checked: " + path + " exists=" + (exists ? "true" : "false"));
    return exists;
}

std::string get_mime_type(const std::string &path)
{
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
    {
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

const LocationConfig *match_location(
    const std::vector<LocationConfig> &locations,
    const std::string &path)
{
    const LocationConfig *best = NULL;
    size_t bestLen = 0;

    for (size_t i = 0; i < locations.size(); ++i)
    {
        const std::string &loc = locations[i].path;

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
        if (loc.length() > bestLen)
        {
            bestLen = loc.length();
            best = &locations[i];
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

bool is_cgi_request(const LocationConfig &loc, const std::string &uri)
{
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

std::string decode_chunked_body(const std::string &raw)
{
    std::istringstream in(raw);
    std::string decoded;
    std::string line;
    bool first_line = true;
    while (true)
    {
        // Read chunk size line
        if (!std::getline(in, line))
            throw std::runtime_error("400: Malformed chunked body (missing chunk size line)");
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        // Ignore chunk extensions
        size_t semi = line.find(';');
        std::string size_str = (semi == std::string::npos) ? line : line.substr(0, semi);
        size_str.erase(0, size_str.find_first_not_of(" \t"));
        size_str.erase(size_str.find_last_not_of(" \t") + 1);
        if (size_str.empty())
            throw std::runtime_error("400: Malformed chunked body (empty chunk size)");
        int chunk_size = 0;
        std::istringstream iss;
        iss.str(size_str);
        iss >> std::hex >> chunk_size;
        if (iss.fail() || chunk_size < 0)
        {
            if (first_line)
            {
                throw std::runtime_error("400: Malformed chunked body (body does not start with valid chunk size line)");
            }
            else
            {
                throw std::runtime_error("400: Malformed chunked body (invalid chunk size)");
            }
        }
        first_line = false;
        if (chunk_size == 0)
        {
            // Last chunk, expect CRLF after
            if (!std::getline(in, line))
                throw std::runtime_error("400: Malformed chunked body (missing final CRLF)");
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (!line.empty())
                throw std::runtime_error("400: Malformed chunked body (extra data after last chunk)");
            break;
        }
        // Read chunk data
        std::string chunk(chunk_size, '\0');
        in.read(&chunk[0], chunk_size);
        if (in.gcount() != chunk_size)
            throw std::runtime_error("400: Malformed chunked body (incomplete chunk data)");
        decoded += chunk;
        // Expect CRLF after chunk data
        if (!std::getline(in, line))
            throw std::runtime_error("400: Malformed chunked body (missing CRLF after chunk data)");
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty())
            throw std::runtime_error("400: Malformed chunked body (extra data after chunk data)");
    }
    // If any data remains, it's a malformed chunked body
    if (in.peek() != EOF)
    {
        std::string extra;
        std::getline(in, extra);
        if (!extra.empty())
            throw std::runtime_error("400: Malformed chunked body (unexpected data after last chunk)");
    }
    return decoded;
}

bool is_directory(const std::string &path)
{
    struct stat statbuf;
    bool dir = stat(path.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
    Logger::log(LOG_DEBUG, "is_directory", path + " is_directory=" + (dir ? "true" : "false"));
    return dir;
}
// JESS: generates JSON response from directory listing if get request comes from client
std::string generate_directory_listing_json(const std::string& fs_dir) {
    std::ostringstream out;
    out << "{\"ok\":true,\"files\":[";
    DIR* d = opendir(fs_dir.c_str());
    if (!d) { out << "]}"; return out.str(); }
    struct dirent* e;
    bool first = true;
    while ((e = readdir(d))) {
        const char* name = e->d_name;
        if (std::strcmp(name,".")==0 || std::strcmp(name,"..")==0) continue;
        if (!first) out << ",";
        out << "\"" << name << "\"";
        first = false;
    }
    closedir(d);
    out << "]}";
    return out.str();
}


std::string generate_directory_listing(const std::string &dir_path, const std::string &uri_path)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
    {
        Logger::log(LOG_ERROR, "generate_directory_listing", "Failed to open dir: " + dir_path);
        return "<html><body><h1>403 Forbidden</h1></body></html>";
    }

    std::vector<std::string> entries;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue; // skip current and parent dir entries
        entries.push_back(name);
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end());

    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>Index of " << uri_path << "</title>"
         << "<style>body{font-family:sans-serif;}table{width:60%;margin:auto;}th,td{text-align:left;padding:4px;}tr:nth-child(even){background:#f9f9f9;}a{text-decoration:none;}</style>"
         << "</head><body>";
    html << "<h1>Index of " << uri_path << "</h1>";
    html << "<table><tr><th>Name</th></tr>";

    // No parent directory link

    for (size_t i = 0; i < entries.size(); ++i)
    {
        std::string name = entries[i];
        std::string href = uri_path;
        if (!href.empty() && href[href.length() - 1] != '/')
            href += "/";
        href += name;
        html << "<tr><td><a href=\"" << href << "\">" << name << "</a></td></tr>";
    }
    html << "</table></body></html>";
    Logger::log(LOG_DEBUG, "generate_directory_listing", "Generated listing for: " + dir_path);
    return html.str();
}

std::string sanitize_filename(const std::string &in)
{
    std::string out;
    for (size_t i = 0; i < in.size(); ++i)
    {
        char c = in[i];
        if (c == '/' || c == '\\')
            continue;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')
        {
            out += c;
        }
    }
    return out.empty() ? std::string("upload") : out;
}

void split_basename_ext(const std::string &name, std::string &base, std::string &ext)
{
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
    {
        base = name;
        ext = "";
    }
    else
    {
        base = name.substr(0, dot);
        ext = name.substr(dot);
    }
}

std::string get_boundary_from_content_type(const std::string &contentType)
{
    const std::string key = "boundary=";
    size_t pos = contentType.find(key);
    if (pos == std::string::npos)
        return "";
    std::string b = contentType.substr(pos + key.size());
    if (!b.empty() && b[0] == '"' && b[b.size() - 1] == '"')
        b = b.substr(1, b.size() - 2);
    return b;
}

bool extract_multipart_file_raw(const std::string &body,
                                const std::string &boundary,
                                std::string &outFilename,
                                std::string &outContent)
{
    if (boundary.empty())
        return false;

    const std::string dashBoundary = std::string("--") + boundary;
    const std::string CRLF = "\r\n";

    // Find first boundary line
    size_t pos = body.find(dashBoundary + CRLF);
    if (pos == std::string::npos)
        return false;
    pos += dashBoundary.size() + CRLF.size();

    while (true)
    {
        // Find headers end
        size_t headersEnd = body.find(CRLF + CRLF, pos);
        if (headersEnd == std::string::npos)
            return false;

        std::string headers = body.substr(pos, headersEnd - pos);

        // Parse filename from Content-Disposition
        std::string filename;
        size_t cd = headers.find("Content-Disposition:");
        if (cd != std::string::npos)
        {
            size_t fn = headers.find("filename=", cd);
            if (fn != std::string::npos)
            {
                size_t q1 = headers.find('"', fn);
                if (q1 != std::string::npos)
                {
                    size_t q2 = headers.find('"', q1 + 1);
                    if (q2 != std::string::npos)
                    {
                        filename = headers.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
        }

        size_t contentStart = headersEnd + 4; // skip \r\n\r\n

        // Next boundary (either middle or closing)
        size_t next = body.find(CRLF + dashBoundary, contentStart);
        if (next == std::string::npos)
        {
            // Try without preceding CRLF (edge case)
            next = body.find(dashBoundary, contentStart);
            if (next == std::string::npos)
                return false;
        }

        // Content ends just before the CRLF preceding the next boundary (if present)
        size_t contentEnd = next;
        if (contentEnd >= 2 && body.substr(contentEnd - 2, 2) == CRLF)
            contentEnd -= 2;

        // If this part has a filename, we treat it as the file part
        if (!filename.empty())
        {
            // strip any path components
            size_t slash = filename.find_last_of("/\\");
            if (slash != std::string::npos)
                filename = filename.substr(slash + 1);

            outFilename = sanitize_filename(filename);
            outContent.assign(body.data() + contentStart, body.data() + contentEnd);
            return true;
        }

        // Move to the start of the next part
        size_t afterBoundary = body.find(CRLF, next + dashBoundary.size());
        if (afterBoundary == std::string::npos)
            return false;
        // closing boundary ends with "--"
        if (body.compare(next, dashBoundary.size() + 2, dashBoundary + "--") == 0)
            return false; // reached end without finding a file
        pos = afterBoundary + CRLF.size();
    }
}

// 1) Check "Transfer-Encoding: chunked" in a header substring (case-insensitive)
bool has_chunked_encoding(const std::string &headers)
{
    std::string h = headers; // copy
    for (size_t i = 0; i < h.size(); ++i)
    {
        char c = h[i];
        if (c >= 'A' && c <= 'Z')
            h[i] = char(c - 'A' + 'a');
    }
    return h.find("transfer-encoding:") != std::string::npos && h.find("chunked") != std::string::npos;
}

// 2) Find the end of a chunked body quickly (looks for the 0-chunk terminator)
size_t find_chunked_terminator(const std::string &buf, size_t body_start)
{
    size_t pos = body_start;

    while (pos < buf.size())
    {
        // Find the next chunk size line
        size_t line_end = buf.find("\r\n", pos);
        if (line_end == std::string::npos)
        {
            return std::string::npos; // need more data
        }

        // Extract chunk size line
        std::string chunk_line = buf.substr(pos, line_end - pos);

        // Parse chunk size (ignore chunk extensions after semicolon)
        size_t semicolon_pos = chunk_line.find(';');
        if (semicolon_pos != std::string::npos)
        {
            chunk_line = chunk_line.substr(0, semicolon_pos);
        }

        // Convert hex chunk size
        size_t chunk_size = 0;
        std::istringstream iss(chunk_line);
        iss >> std::hex >> chunk_size;

        if (iss.fail())
        {
            return std::string::npos; // invalid chunk size
        }

        // If chunk size is 0, this is the final chunk
        if (chunk_size == 0)
        {
            // Look for the final \r\n\r\n after possible trailing headers
            size_t final_pos = line_end + 2; // after chunk size line

            // Skip any trailing headers
            while (final_pos < buf.size())
            {
                size_t header_end = buf.find("\r\n", final_pos);
                if (header_end == std::string::npos)
                {
                    return std::string::npos; // need more data
                }

                // If we found an empty line, we're done
                if (header_end == final_pos)
                {
                    return header_end + 2; // return position after final \r\n
                }

                final_pos = header_end + 2;
            }
            return std::string::npos; // need more data for final \r\n
        }

        // Skip to after this chunk's data and trailing \r\n
        pos = line_end + 2 + chunk_size + 2; // chunk_size + data + \r\n

        if (pos > buf.size())
        {
            return std::string::npos; // need more data
        }
    }

    return std::string::npos; // need more data
}
