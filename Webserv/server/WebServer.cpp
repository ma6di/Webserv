#include "WebServer.hpp"
#include "Config.hpp"
#include <cstring>
#include <ctime>
#include <errno.h>

WebServer::WebServer(const Config &cfg)
    : config_(&cfg)
{
    std::vector<int> ports = config_->getPorts();
    for (size_t idx = 0; idx < ports.size(); ++idx)
    {
        int port = ports[idx];
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("socket");
            continue;
        }

        make_socket_non_blocking(sock);

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            close(sock);
            continue;
        }

        if (listen(sock, SOMAXCONN) < 0)
        {
            perror("listen");
            close(sock);
            continue;
        }

        Logger::log(LOG_INFO, "WebServer",
                    "Server listening on http://localhost:" + to_str(port));
        listening_sockets.push_back(sock);
    }
}

WebServer::~WebServer()
{
    shutdown();
}

void WebServer::shutdown()
{
    for (size_t i = 0; i < listening_sockets.size(); ++i)
    {
        ::close(listening_sockets[i]);
    }
    listening_sockets.clear();

    for (std::map<int, Connection>::iterator it = conns_.begin();
         it != conns_.end(); ++it)
    {
        ::close(it->first);
    }
    conns_.clear();

    Logger::log(LOG_INFO, "WebServer", "All sockets closed.");
}

void WebServer::make_socket_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("Failed to make socket non-blocking");
}

int WebServer::handleNewConnection(int listen_fd)
{
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(listen_fd, (sockaddr *)&client_addr, &addrlen);
    if (client_fd < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return -1;
    }

    make_socket_non_blocking(client_fd);
    conns_[client_fd]; // Create new connection (last_active already set in constructor)
    Logger::log(LOG_INFO, "WebServer", "Accepted FD=" + to_str(client_fd));
    return client_fd;
}

void WebServer::handleClientDataOn(int client_fd)
{
    char buf[4096];
    ssize_t n = ::read(client_fd, buf, sizeof(buf));
    if (n <= 0)
    {
        // client closed or error
        ::close(client_fd);
        conns_.erase(client_fd);
        Logger::log(LOG_INFO, "WebServer", "Closed FD=" + to_str(client_fd));
        return;
    }

    // Update client activity timestamp
    updateClientActivity(client_fd);

    // append into our buffer
    std::string &data = conns_[client_fd].readBuf;
    data.append(buf, (size_t)n);
    // ...existing code...

    // do we have a complete header yet?
    for (;;) { // loop to handle pipelined requests in the same buffer
        size_t hdr_end = find_header_end(data);
        if (hdr_end == std::string::npos) {
            // need more bytes for headers
            return;
        }

        const size_t header_bytes = hdr_end + 4;
        std::string headers = data.substr(0, header_bytes);

        size_t needed = header_bytes;
        if (has_chunked_encoding(headers)) {
            if (data.size() <= header_bytes) {
                // wait for some body
                return;
            }
            Logger::log(LOG_DEBUG, "WebServer", "Detected chunked encoding, passing frame to Request for validation");
            needed = data.size();
        } else {
            int len = parse_content_length(headers);
            if (len < 0) len = 0;
            // Wait for exactly Content-Length bytes before parsing
            if (data.size() < header_bytes + (size_t)len) {
                // wait for more body bytes
                return;
            }
            needed = header_bytes + (size_t)len;
        }

        // Slice exactly one full request frame
        std::string frame = data.substr(0, needed);

        try {
            Request req(frame);                 // parse only the frame
            process_request(req, client_fd, 0); // your existing function
        } catch (const std::exception& e) {
            std::string error_msg = e.what();
            Logger::log(LOG_ERROR, "WebServer",
                        std::string("Request parse failed: ") + error_msg);
            // Check for specific error types
            if (error_msg.find("HTTP Version Not Supported") != std::string::npos) {
                send_error_response(client_fd, 505, "HTTP Version Not Supported", 0);
                flushPendingWrites(client_fd);
                conns_[client_fd].shouldCloseAfterWrite = true;
            } else if (error_msg.find("501:") == 0) {
                send_error_response(client_fd, 501, "Not Implemented", 0);
                flushPendingWrites(client_fd);
                conns_[client_fd].shouldCloseAfterWrite = true;
            } else {
                send_error_response(client_fd, 400, "Bad Request", 0);
                flushPendingWrites(client_fd);
                conns_[client_fd].shouldCloseAfterWrite = true;
            }
        }

        // Consume the bytes we just handled; if keep-alive and more data
        // already arrived, loop to process the next request.
        data.erase(0, needed);
        if (data.empty()) {
            return;
        }
        // else continue the for(;;) to try parse next request in buffer
    }
}

// --- Helper: Process the request ---
void WebServer::process_request(Request &request, int client_fd, size_t i)
{
    std::string ver = request.getVersion();
    std::string connHdr = request.getHeader("Connection");
    bool close_conn = (connHdr == "close") || (ver == "HTTP/1.0" && connHdr != "keep-alive");
    conns_[client_fd].shouldCloseAfterWrite = close_conn;

    Logger::log(LOG_INFO, "POLICY",
        "fd=" + to_str(client_fd) +
        " path=" + request.getPath() +
        " ver=" + ver +
        " conn=" + (connHdr.empty() ? std::string("<none>") : connHdr) +
        " closeAfter=" + (close_conn ? "true" : "false"));

    // Handle Expect: 100-continue header
    if (request.hasExpectContinue()) {
        Logger::log(LOG_INFO, "WebServer", "Client expects 100-continue, sending 100 Continue response");
        send_continue_response(client_fd);
        // Note: Client will send the body after receiving 100 Continue
        // The request body should already be parsed by the Request constructor
    }

    std::string method = request.getMethod();
    std::string uri = request.getPath();
    const LocationConfig *loc = match_location(config_->getLocations(), uri);

    if (loc)
        Logger::log(LOG_DEBUG, "WebServer", "Matched location: " + loc->path);
    else
        Logger::log(LOG_DEBUG, "WebServer", "No location matched!");

    // General error: 501 Not Implemented
    if (method != "GET" && method != "POST" && method != "DELETE") {
        send_error_response(client_fd, 501, "Not Implemented", i);
        return;
    }

    // General error: 413 Payload Too Large
    if (request.getBody().size() > config_->getMaxBodySize()) {
        conns_[client_fd].shouldCloseAfterWrite = true;
        send_error_response(client_fd, 413, "Payload Too Large", i);
        return;
    }

    // General error: 405 Method Not Allowed
    if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end()) {
        send_error_response(client_fd, 405, "Method Not Allowed", i);
        return;
    }

    // CGI check for GET/DELETE (not POST)
    int is_cgi = (loc && !loc->cgi_extension.empty() && is_cgi_request(*loc, request.getPath())) ? 1 : 0;
    if ((method == "GET" || method == "DELETE") && loc && is_cgi) {
        Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
        handle_cgi(loc, request, client_fd, i);
        return;
    }

    // Redirect check
    if (loc && !loc->redirect_url.empty()) {
        const std::string hostHdr = request.getHeader("Host");
        const bool external = isExternalRedirect(loc->redirect_url, hostHdr);
        if (external) {
            conns_[client_fd].shouldCloseAfterWrite = true;
            Logger::log(LOG_INFO, "redirect", "External → " + loc->redirect_url + " (will close)");
        } else {
            Logger::log(LOG_INFO, "redirect", "Internal → " + loc->redirect_url + " (keep-alive)");
        }
        send_redirect_response(client_fd, loc->redirect_code == 0 ? 301 : loc->redirect_code, loc->redirect_url, i);
        if (!conns_[client_fd].shouldCloseAfterWrite) {
            conns_[client_fd].readBuf.clear();
            Logger::log(LOG_DEBUG, "RESET", "fd=" + to_str(client_fd) + " cleared readBuf after internal redirect");
        }
        return;
    }

    Logger::log(LOG_INFO, "request", "Ver=" + request.getVersion() + " ConnHdr=" + request.getHeader("Connection"));

    // Method-specific error checks and handling
    if (method == "GET") {
        handle_get(request, loc, client_fd, i);
    } else if (method == "POST") {
        if (!validate_post_request(request, client_fd, i)) {
            return;
        }
        // CGI check for POST
        if (loc && is_cgi) {
            Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
            handle_cgi(loc, request, client_fd, i);
            return;
        }
        Logger::log(LOG_DEBUG, "process_request",
            "POST " + request.getPath() +
            " matched to location " + (loc ? loc->path : "NULL") +
            " upload_dir=" + (loc ? loc->upload_dir : "<none>"));
        handle_post(request, loc, client_fd, i);
    } else if (method == "DELETE") {
        handle_delete(request, loc, client_fd, i);
    }
    if (!conns_[client_fd].shouldCloseAfterWrite) {
        conns_[client_fd].readBuf.clear();
        Logger::log(LOG_DEBUG, "RESET", "fd=" + to_str(client_fd) + " keeping alive; cleared readBuf");
    }
}

// Helper for POST validation
bool WebServer::validate_post_request(Request &request, int client_fd, size_t i) {
    long contentLength = request.getContentLength();
    bool isChunked = request.isChunked();
    // Must have either Content-Length or Transfer-Encoding: chunked, but not both
    if (contentLength && isChunked) {
		std::cout << "both\n";
        send_error_response(client_fd, 400, "Bad Request", i);
        flushPendingWrites(client_fd);
        return false;
    }
    if (!contentLength && !isChunked) {
        send_error_response(client_fd, 411, "Length Required", i);
        return false;
    }

    // If Content-Length, it must match body size
    if (contentLength) {
        if (contentLength < 0 || contentLength != static_cast<long>(request.getBody().size())) {
            std::cout << "cn pro\n";
			send_error_response(client_fd, 400, "Bad Request", i);
            return false;
        }
    }
    // No chunked decoding here; handled in Request.cpp
    return true;
}

void WebServer::cleanup_client(int client_fd, int i)
{
    (void)i;
    ::close(client_fd);
    conns_.erase(client_fd); 
    Logger::log(LOG_INFO, "WebServer", "Cleaned up client FD=" + to_str(client_fd));
}

void WebServer::handle_new_connection(int listen_fd)
{
    handleNewConnection(listen_fd);
}

void WebServer::queueResponse(int client_fd,
                              const std::string &rawResponse)
{
    Connection &conn = conns_[client_fd];
    conn.writeBuf += rawResponse;
}

bool WebServer::hasPendingWrite(int client_fd) const
{
    std::map<int, Connection>::const_iterator it = conns_.find(client_fd);
    if (it == conns_.end())
        return false;
    return !it->second.writeBuf.empty();
}

void WebServer::flushPendingWrites(int client_fd)
{
    std::map<int, Connection>::iterator it = conns_.find(client_fd);
    if (it == conns_.end())
        return;
    Connection &conn = it->second;

    while (!conn.writeBuf.empty())
    {
        ssize_t n = ::write(client_fd,
                            conn.writeBuf.data(),
                            conn.writeBuf.size());
        if (n > 0)
        {
            conn.writeBuf.erase(0, static_cast<size_t>(n));
            // Update activity timestamp when we successfully write data
            updateClientActivity(client_fd);
        }
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }
        else
        {
            cleanup_client(client_fd, 0);
            return;
        }
    }

    if (conn.writeBuf.empty() && conn.shouldCloseAfterWrite)
    {
        Logger::log(LOG_DEBUG, "flush",
                    "fd=" + to_str(client_fd) + " drained; closing");
        ::shutdown(client_fd, SHUT_WR);
        cleanup_client(client_fd, 0);
    }
    else
    {
        Logger::log(LOG_DEBUG, "flush", "fd=" + to_str(client_fd) + " drained; keeping open");
    }
}

// Timeout management methods
time_t WebServer::getClientLastActive(int client_fd) const
{
    std::map<int, Connection>::const_iterator it = conns_.find(client_fd);
    if (it == conns_.end())
        return 0;
    return it->second.last_active;
}

void WebServer::updateClientActivity(int client_fd)
{
    std::map<int, Connection>::iterator it = conns_.find(client_fd);
    if (it != conns_.end()) {
        it->second.last_active = time(NULL);
    }
}

void WebServer::closeClient(int client_fd)
{
    std::map<int, Connection>::iterator it = conns_.find(client_fd);
    if (it != conns_.end()) {
        ::close(client_fd);
        conns_.erase(it);
        Logger::log(LOG_INFO, "timeout", "Closed client fd=" + to_str(client_fd) + " due to timeout");
    }
}

bool WebServer::isAbsoluteHttpUrl(const std::string &s)
{
    return (s.compare(0, 7, "http://") == 0) ||
           (s.compare(0, 8, "https://") == 0) ||
           (s.compare(0, 2, "//") == 0); 
}

std::string WebServer::hostportFromUrl(const std::string &url)
{

    std::string s = url;
    size_t start = 0;
    if (s.compare(0, 2, "//") == 0)
    {
        start = 2;
    }
    else
    {
        size_t p = s.find("://");
        if (p != std::string::npos)
            start = p + 3;
    }
    size_t end = s.find('/', start);
    if (end == std::string::npos)
        end = s.size();
    return s.substr(start, end - start); 
}

bool WebServer::iequals(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        char ca = a[i], cb = b[i];
        if ('A' <= ca && ca <= 'Z')
            ca = char(ca - 'A' + 'a');
        if ('A' <= cb && cb <= 'Z')
            cb = char(cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return true;
}

bool WebServer::isExternalRedirect(const std::string &location, const std::string &reqHost)
{
    if (!location.empty() && location[0] == '/')
        return false;

    if (isAbsoluteHttpUrl(location))
    {
        std::string hp = hostportFromUrl(location);
        if (reqHost.empty())
            return true;
        return !iequals(hp, reqHost);
    }

    return true;
}
