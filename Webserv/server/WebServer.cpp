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
#include <cctype> // for isxdigit

extern Config g_config;

/*WebServer::WebServer(int port) : port(port) {
    setup_server_socket(port);
}*/

WebServer::WebServer(const std::vector<int>& ports) {
    for (size_t i = 0; i < ports.size(); ++i) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            continue;
        }

        fcntl(sockfd, F_SETFL, O_NONBLOCK);  // Make non-blocking

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

        std::cout << "Server running on http://localhost:" << ports[i] << std::endl;
        listening_sockets.push_back(sockfd);
    }
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

    for (size_t i = 0; i < listening_sockets.size(); ++i) {
	    pollfd pfd;
	    pfd.fd = listening_sockets[i];
	    pfd.events = POLLIN;
	    pfd.revents = 0;
	    fds.push_back(pfd);
    }

    size_t listener_count = listening_sockets.size();
    std::cout << "[DEBUG] Listening sockets count: " << listener_count << std::endl;
	
    while (true) {
		int count = poll(&fds[0], fds.size(), -1);
		if (count < 0) {
			std::cerr << "[ERROR] Poll failed\n";
			break;
		}

		for (size_t i = 0; i < fds.size(); ++i) {
	        if (!(fds[i].revents & POLLIN))
		        continue;
	        if (i < listener_count) {
    		    handle_new_connection(fds[i].fd);  // pass the correct listener FD
	        } else {
		        handle_client_data(i--);  // This may remove the client, so i-- is correct
	        }
        }
	}
}

/*void WebServer::handle_new_connection() {
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
}*/

void WebServer::handle_new_connection(int listen_fd) {
	sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addr_len);
	if (client_fd < 0) {
		perror("accept");
		return;
	}

	fcntl(client_fd, F_SETFL, O_NONBLOCK);  // Optional but useful

	pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	fds.push_back(pfd);

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
    try {
        Request request(request_data);
        std::cout << "[DEBUG] max body size: "<< g_config.getMaxBodySize() << std::endl;
        // Check for chunked encoding and content length
        std::string transfer_encoding = request.getHeader("Transfer-Encoding");
        bool is_chunked = !transfer_encoding.empty() && transfer_encoding == "chunked";
        int content_length = parse_content_length(request_data.substr(0, header_end));
        size_t body_start = header_end + 4;
        size_t body_length_received = request_data.size() - body_start;
        // Wait for more data if needed
        if (!is_chunked && content_length > 0 && body_length_received < static_cast<size_t>(content_length)) {
            return;
        }
        if (is_chunked && request.getBody().empty()) {
            return;
        }
        std::cout << "[DEBUG] Full request received. Parsing and processing..." << std::endl;
        std::string method = request.getMethod();
        std::string uri = request.getPath();
        const LocationConfig* loc = match_location(g_config.getLocations(), uri);
        if (loc)
            std::cout << "[DEBUG] Matched location: " << loc->path << std::endl;
        else
            std::cout << "[DEBUG] No location matched!" << std::endl;
        // 501 Not Implemented
        if (method != "GET" && method != "POST" && method != "DELETE") {
            Response resp(501, "Not Implemented");
            write(client_fd, resp.toString().c_str(), resp.toString().size());
            cleanup_client(client_fd, i);
            return;
        }
        // --- DECODE CHUNKED BODY IF NEEDED ---
        if (is_chunked) {
            std::string decoded = decode_chunked_body(request.getBody());
            request.setBody(decoded);
        }
        std::cout << "[DEBUG] max body size: "<< g_config.getMaxBodySize() << std::endl;
        // 413 Payload Too Large
        if (request.getBody().size() > g_config.getMaxBodySize()) {
            Response resp(413, "Payload Too Large");
            //resp.setBody("<h1>413 Payload Too Large</h1>");
            std::string raw = resp.toString();
            write(client_fd, raw.c_str(), raw.size());
            cleanup_client(client_fd, i);
            return;
        }
        // 405 Method Not Allowed
        if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end()) {
            Response resp(405, "Method Not Allowed");
            write(client_fd, resp.toString().c_str(), resp.toString().size());
            cleanup_client(client_fd, i);
            return;
        }
        std::cout << "[DEBUG] is_cgi_request: " << is_cgi_request(*loc, request.getPath()) << std::endl;
        // Handle CGI
        if (loc && is_cgi_request(*loc, request.getPath())) {
            handle_cgi(loc, request, client_fd, i);
            return;
        }
        // Static file/auth/permission checks (401/403/404)
        if (method == "GET") {
            handle_get(request, loc, client_fd, i);
        } else if (method == "POST") {
            handle_post(request, loc, client_fd, i);
        } else if (method == "DELETE") {
            handle_delete(request, client_fd, i);
        }
        std::string connection_header = request.getHeader("Connection");
        bool close_connection = false;
        if (connection_header == "close" || request.getVersion() == "HTTP/1.0") {
            close_connection = true;
        }
        if (close_connection) {
            cleanup_client(client_fd, i);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Request parsing failed: " << e.what() << std::endl;
        Response resp(400, "Bad Request");
        write(client_fd, resp.toString().c_str(), resp.toString().size());
        cleanup_client(client_fd, i);
    }
    client_buffers.erase(client_fd);
}

/*void WebServer::handle_client_data(size_t i) {
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

    try {
        Request request(request_data);

        std::cout << "[DEBUG] max body size: "<< g_config.getMaxBodySize() << std::endl;

        // Check for chunked encoding and content length
        std::string transfer_encoding = request.getHeader("Transfer-Encoding");
        bool is_chunked = !transfer_encoding.empty() && transfer_encoding == "chunked";
        int content_length = parse_content_length(request_data.substr(0, header_end));
        size_t body_start = header_end + 4;
        size_t body_length_received = request_data.size() - body_start;

        // Wait for more data if needed
        if (!is_chunked && content_length > 0 && body_length_received < static_cast<size_t>(content_length)) {
            return;
        }
        if (is_chunked && request.getBody().empty()) {
            return;
        }

        std::cout << "[DEBUG] Full request received. Parsing and processing..." << std::endl;

        std::string method = request.getMethod();
        std::string uri = request.getPath();
        const LocationConfig* loc = match_location(g_config.getLocations(), uri);

		if (loc)
    		std::cout << "[DEBUG] Matched location: " << loc->path << std::endl;
		else
			std::cout << "[DEBUG] No location matched!" << std::endl;
        // 501 Not Implemented
        if (method != "GET" && method != "POST" && method != "DELETE") {
            Response resp(501, "Not Implemented");
            write(client_fd, resp.toString().c_str(), resp.toString().size());
            cleanup_client(client_fd, i);
            return;
        }

        // --- DECODE CHUNKED BODY IF NEEDED ---
        if (is_chunked) {
            std::string decoded = decode_chunked_body(request.getBody());
            request.setBody(decoded);
        }

        std::cout << "[DEBUG] max body size: "<< g_config.getMaxBodySize() << std::endl;
        // 413 Payload Too Large
		if (request.getBody().size() > g_config.getMaxBodySize()) {
			Response resp(413, "Payload Too Large");
			//resp.setBody("<h1>413 Payload Too Large</h1>");
			std::string raw = resp.toString();
			write(client_fd, raw.c_str(), raw.size());
			cleanup_client(client_fd, i);
			return;
		}

        // 405 Method Not Allowed
        if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end()) {
            Response resp(405, "Method Not Allowed");
            write(client_fd, resp.toString().c_str(), resp.toString().size());
            cleanup_client(client_fd, i);
            return;
        }

		std::cout << "[DEBUG] is_cgi_request: " << is_cgi_request(*loc, request.getPath()) << std::endl;
        // Handle CGI
        if (loc && is_cgi_request(*loc, request.getPath())) {
            handle_cgi(loc, request, client_fd, i);
            return;
        }

        // Static file/auth/permission checks (401/403/404)
        if (method == "GET") {
            handle_get(request, loc, client_fd, i);
        } else if (method == "POST") {
            handle_post(request, loc, client_fd, i);
        } else if (method == "DELETE") {
            handle_delete(request, client_fd, i);
        }

        cleanup_client(client_fd, i);
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Request parsing failed: " << e.what() << std::endl;
        Response resp(400, "Bad Request");
        write(client_fd, resp.toString().c_str(), resp.toString().size());
        cleanup_client(client_fd, i);
    }

    client_buffers.erase(client_fd);
}*/
