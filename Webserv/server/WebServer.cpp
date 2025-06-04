#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm> // 🔧 Needed for std::find

extern Config g_config;

WebServer::WebServer(int port) : port(port) {
    setup_server_socket(port);
}

WebServer::~WebServer() {
    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
}

void WebServer::run() {
    std::cout << "Server running on http://localhost:" << port << "\n";
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
    fds.clear();
    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    while (true) {
        int count = poll(&fds[0], fds.size(), -1);
        if (count < 0) {
            std::cerr << "Poll failed\n";
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN)) continue;

            if (fds[i].fd == server_fd)
                handle_new_connection();
            else
                handle_client_data(i--);
        }
    }
}

void WebServer::handle_new_connection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Accept failed\n";
        return;
    }

    make_socket_non_blocking(client_fd);

    struct pollfd client_pfd;
    client_pfd.fd = client_fd;
    client_pfd.events = POLLIN;
    client_pfd.revents = 0;
    fds.push_back(client_pfd);

    std::cout << "New client connected: FD=" << client_fd << "\n";
}

void WebServer::handle_client_data(size_t i) {
    int client_fd = fds[i].fd;
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    int bytes = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes <= 0) {
        std::cout << "Client disconnected: FD=" << client_fd << "\n";
        close(client_fd);
        fds.erase(fds.begin() + i);
        return;
    }

    try {
		Request request = Request(std::string(buffer));
        std::string uri = request.getPath();
        const LocationConfig* loc = match_location(g_config.getLocations(), uri);

        if (loc) {
            const std::vector<std::string>& allowed = loc->allowed_methods;
            std::string method = request.getMethod();
            if (std::find(allowed.begin(), allowed.end(), method) == allowed.end()) {
                send_error_response(client_fd, 405, "Method Not Allowed", i);
                return;
            }
        }

        if (loc && is_cgi_request(*loc, uri)) {
            std::string script_path = resolve_script_path(uri, *loc);

            std::map<std::string, std::string> env;
            env["REQUEST_METHOD"] = request.getMethod();
            env["SCRIPT_NAME"] = uri;
            env["QUERY_STRING"] = "";
            std::ostringstream oss;
            oss << request.getBody().size();
            env["CONTENT_LENGTH"] = oss.str();

            CGIHandler handler(script_path, env, request.getBody());
            std::string cgi_output = handler.execute();

            write(client_fd, cgi_output.c_str(), cgi_output.size());
        } else {
            if (!file_exists(resolve_path(uri))) {
                send_error_response(client_fd, 404, "Not Found", i);
                return;
            }
            send_response(client_fd, uri);
        }

    } catch (const std::exception& e) {
        std::cerr << "Request parse error: " << e.what() << "\n";
        send_error_response(client_fd, 400, "Bad Request", i);
        return;
    }

    close(client_fd);
    fds.erase(fds.begin() + i);
}


// Serve a static file
void WebServer::send_response(int client_fd, const std::string& raw_path) {
    std::string file_path = resolve_path(raw_path);
    std::ifstream file(file_path.c_str());
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();

    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << get_mime_type(file_path) << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string response = oss.str();
    write(client_fd, response.c_str(), response.size());
}

// Load a custom or default error response and send it
void WebServer::send_error_response(int client_fd, int status_code, const std::string& status_text, size_t index) {
    std::string body;
    const std::string* errorPage = g_config.getErrorPage(status_code);

    if (errorPage && g_config.pathExists(*errorPage)) {
        std::ifstream file(errorPage->c_str());
        std::ostringstream content;
        content << file.rdbuf();
        body = content.str();
    } else {
        std::ostringstream fallback;
        fallback << "<h1>" << status_code << " " << status_text << "</h1>";
        body = fallback.str();
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string response = oss.str();
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
    fds.erase(fds.begin() + index);
}

// Convert URI to full file path
std::string WebServer::resolve_path(const std::string& raw_path) {
    std::string path = raw_path;

    if (path == "/")
        path = "/index.html";
    else if (!path.empty() && path[path.size() - 1] == '/')
        path += "index.html";

    std::string full_path = "./www" + path;

    if (file_exists(full_path))
        return full_path;

    if (path.find('.') == std::string::npos) {
        std::string fallback = "./www" + path + ".html";
        if (file_exists(fallback))
            return fallback;
    }

    return full_path;
}
