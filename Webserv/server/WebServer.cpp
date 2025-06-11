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
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include <ctime>


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
	std::cout << "[DEBUG] poll_loop running, fds.size() = " << fds.size() << std::endl;
	fds.clear();

	pollfd pfd;
	pfd.fd = server_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	fds.push_back(pfd);

	while (true) {
		int count = poll(&fds[0], fds.size(), -1);
		if (count < 0) {
			std::cerr << "[ERROR] Poll failed\n";
			break;
		}

		for (size_t i = 0; i < fds.size(); ++i) {
			if (!(fds[i].revents & POLLIN))
				continue;

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
		std::cerr << "[ERROR] Accept failed\n";
		return;
	}

	make_socket_non_blocking(client_fd);

	pollfd client_pfd;
	client_pfd.fd = client_fd;
	client_pfd.events = POLLIN;
	client_pfd.revents = 0;
	fds.push_back(client_pfd);

	std::cout << "[INFO] New client connected: FD=" << client_fd << std::endl;
}

void WebServer::cleanup_client(int fd, size_t i) {
    // Close the client socket file descriptor
    close(fd);

    // Remove the pollfd entry from fds vector
    if (i < fds.size()) {
        fds.erase(fds.begin() + i);
    }

    // Remove the client's buffer from the map
    client_buffers.erase(fd);

    std::cout << "[INFO] Cleaned up client FD=" << fd << std::endl;
}


void WebServer::shutdown() {
    // Close all open sockets
    for (size_t i = 0; i < fds.size(); ++i) {
        close(fds[i].fd);
    }
    fds.clear();
    std::cout << "WebServer: All sockets closed.\n";
}

void WebServer::handle_client_data(size_t i) {
    int client_fd = fds[i].fd;
    std::cout << "[DEBUG] handle_client_data: FD=" << client_fd << std::endl;

    if (!read_and_append_client_data(client_fd, i))
        return;

    std::string& request_data = client_buffers[client_fd];
    size_t header_end = find_header_end(request_data);
    if (header_end == std::string::npos) {
        std::cout << "[DEBUG] Incomplete headers. Waiting for more data..." << std::endl;
        return;
    }

    std::string headers = request_data.substr(0, header_end);
    size_t body_start = header_end + 4;
    size_t body_length_received = request_data.size() - body_start;

    int content_length = parse_content_length(headers);

    if (content_length > 0 && body_length_received < static_cast<size_t>(content_length)) {
        std::cout << "[DEBUG] Incomplete body. Waiting for more data..." << std::endl;
        return;
    }

    std::cout << "[DEBUG] Full request received. Parsing and processing..." << std::endl;

    try {
        Request request(request_data);
        std::string method = request.getMethod();
        std::string uri = request.getPath();
        const LocationConfig* loc = match_location(g_config.getLocations(), uri);

        // 501 Not Implemented
        if (method != "GET" && method != "POST" && method != "DELETE") {
            send_error_response(client_fd, 501, "Not Implemented", i);
            cleanup_client(client_fd, i);
            return;
        }

        // 413 Payload Too Large
        if (request.getBody().size() > g_config.getMaxBodySize()) {
            send_error_response(client_fd, 413, "Payload Too Large", i);
            cleanup_client(client_fd, i);
            return;
        }

        // 405 Method Not Allowed
        if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end()) {
            std::cout << "method: "<<method <<"\n";
			std::cout << "Allowed methods for this location: ";
			for (size_t j = 0; j < loc->allowed_methods.size(); ++j)
				std::cout << loc->allowed_methods[j] << " ";
			std::cout << std::endl;
			send_error_response(client_fd, 405, "Method Not Alloweddd", i);
            cleanup_client(client_fd, i);
            return;
        }

		if (loc && is_cgi_request(*loc, request.getPath())) {
			std::cout << "🔍 Handling CGI for POST request\n";
			handle_cgi(loc, request, client_fd, i);
        return;
    }
        // Dispatch to method handlers
        if (method == "GET") {
            handle_get(request, client_fd, i);
        } else if (method == "POST") {
            handle_post(request, loc, client_fd, i);
        } else if (method == "DELETE") {
            handle_delete(request, client_fd, i);
        }

        cleanup_client(client_fd, i);
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Request parsing failed: " << e.what() << std::endl;
        send_error_response(client_fd, 400, "Bad Request", i);
        cleanup_client(client_fd, i);
    }

    client_buffers.erase(client_fd);
}
