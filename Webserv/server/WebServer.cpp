/**
 * WebServer.cpp
 * -------------
 * Implements the WebServer class.
 * - Sets up listening sockets
 * - Accepts and manages client connections
 * - Handles HTTP requests, static files, uploads, and CGI
 * - Provides logging and error handling
 */

#include "WebServer.hpp"

extern Config g_config;

WebServer::WebServer(const std::vector<int>& ports) {
    for (size_t i = 0; i < ports.size(); ++i) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            continue;
        }

        fcntl(sockfd, F_SETFL, O_NONBLOCK);

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(ports[i]);

        if (bind(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            close(sockfd);
            continue;
        }

        if (listen(sockfd, SOMAXCONN) < 0) {
            perror("listen");
            close(sockfd);
            continue;
        }

        Logger::log(LOG_INFO, "WebServer", "Server running on http://localhost:" + to_str(ports[i]));
        listening_sockets.push_back(sockfd);
    }
}

WebServer::~WebServer() {
    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
}

void WebServer::run() {
    poll_loop();
}

void WebServer::setup_server_socket(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        throw std::runtime_error("Socket creation failed");

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("Bind failed");
    if (listen(server_fd, 10) < 0)
        throw std::runtime_error("Listen failed");

    make_socket_non_blocking(server_fd);
}

void WebServer::make_socket_non_blocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set O_NONBLOCK");
}

void WebServer::poll_loop() {
    Logger::log(LOG_DEBUG, "WebServer", "poll_loop running, fds.size() = " + to_str(fds.size()));
    fds.clear();

    for (size_t i = 0; i < listening_sockets.size(); ++i) {
        pollfd pfd;
        pfd.fd = listening_sockets[i];
        pfd.events = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
    }

    size_t listener_count = listening_sockets.size();
    Logger::log(LOG_DEBUG, "WebServer", "Listening sockets count: " + to_str(listener_count));

    while (true) {
        int count = poll(&fds[0], fds.size(), -1);
        if (count < 0) {
            Logger::log(LOG_ERROR, "WebServer", "Poll failed");
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;
            if (i < listener_count) {
                handle_new_connection(fds[i].fd);
            } else {
                handle_client_data(i--);
            }
        }
    }
}

void WebServer::handle_new_connection(int listen_fd) {
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    Logger::log(LOG_INFO, "WebServer", "New client connected: FD=" + to_str(client_fd));
}

void WebServer::cleanup_client(int fd, size_t i) {
    close(fd);
    if (i < fds.size()) {
        fds.erase(fds.begin() + i);
    }
    client_buffers.erase(fd);
    Logger::log(LOG_INFO, "WebServer", "Cleaned up client FD=" + to_str(fd));
}

void WebServer::shutdown() {
    for (size_t i = 0; i < fds.size(); ++i) {
        close(fds[i].fd);
    }
    fds.clear();
    Logger::log(LOG_INFO, "WebServer", "All sockets closed.");
}

void WebServer::handle_client_data(size_t i) {
    int client_fd = fds[i].fd;
    Logger::log(LOG_DEBUG, "WebServer", "handle_client_data: FD=" + to_str(client_fd));

    if (!read_and_append_client_data(client_fd, i))
        return;

    std::string& request_data = client_buffers[client_fd];
    size_t header_end = find_header_end(request_data);
    if (header_end == std::string::npos) {
        Logger::log(LOG_DEBUG, "WebServer", "Incomplete headers. Waiting for more data...");
        return;
    }

    try {
        Request request(request_data);
        if (!is_full_body_received(request, request_data, header_end)) {
            return;
        }

        process_request(request, client_fd, i);

        client_buffers.erase(client_fd);
        Logger::log(LOG_DEBUG, "WebServer", "Request processed and buffer erased.");
    }
    catch (const std::exception& e) {
        Logger::log(LOG_ERROR, "WebServer", std::string("Request parsing failed: ") + e.what());
        send_error_response(client_fd, 400, "Bad Request", i);
        client_buffers.erase(client_fd);
    }
	std::cout << std::endl <<std::endl;
}

// --- Helper: Process the request ---
void WebServer::process_request(Request& request, int client_fd, size_t i) {
    std::string method = request.getMethod();
    std::string uri = request.getPath();
    const LocationConfig* loc = match_location(g_config.getLocations(), uri);

    if (loc)
        Logger::log(LOG_DEBUG, "WebServer", "Matched location: " + loc->path);
    else
        Logger::log(LOG_DEBUG, "WebServer", "No location matched!");

    // 501 Not Implemented
    if (method != "GET" && method != "POST" && method != "DELETE") {
        send_error_response(client_fd, 501, "Not Implemented", i);
        return;
    }

    // Decode chunked body if needed
    if (request.isChunked()) {
        std::string decoded = decode_chunked_body(request.getBody());
        request.setBody(decoded);
    }

    // 413 Payload Too Large
    if (request.getBody().size() > g_config.getMaxBodySize()) {
        send_error_response(client_fd, 413, "Payload Too Large", i);
        return;
    }

    // 405 Method Not Allowed
    if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end()) {
        send_error_response(client_fd, 405, "Method Not Allowed", i);
        return;
    }

    // CGI check
    int is_cgi = (loc ? is_cgi_request(*loc, request.getPath()) : 0);
    Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
    if (loc && is_cgi) {
        handle_cgi(loc, request, client_fd, i);
        return;
    }

    // Dispatch to method handler
    if (method == "GET") {
        handle_get(request, loc, client_fd, i);
    } else if (method == "POST") {
        handle_post(request, loc, client_fd, i);
    } else if (method == "DELETE") {
        handle_delete(request, client_fd, i);
    }

    // Connection: close logic
    std::string connection_header = request.getHeader("Connection");
    bool close_connection = (connection_header == "close" || request.getVersion() == "HTTP/1.0");
    if (close_connection) {
        cleanup_client(client_fd, i);
    }
}

