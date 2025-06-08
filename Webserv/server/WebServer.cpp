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

// void WebServer::poll_loop() {
//     while (true) {
//         int ret = poll(fds.data(), fds.size(), -1);
//         if (ret < 0) {
//             perror("poll");
//             break;
//         }

//         // Iterate backwards to safely remove clients while looping
//         for (int i = static_cast<int>(fds.size()) - 1; i >= 0; --i) {
//             if (fds[i].revents & POLLIN) {
//                 if (fds[i].fd == server_fd) {
//                     handle_new_connection();
//                 } else {
//                     handle_client_data(i);
//                 }
//             } else if (fds[i].revents & (POLLHUP | POLLERR)) {
//                 cleanup_client(fds[i].fd, i);
//             }
//             // Reset revents to 0 for next poll cycle
//             fds[i].revents = 0;
//         }
//     }
// }

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

// void WebServer::run_one_iteration() {
// 	std::cout << "[DEBUG] at top of one iterations "<< std::endl;

//     // Wait for events with a timeout (e.g., 1000 ms)
//     int ret = poll(fds.data(), fds.size(), 1000);
//     if (ret < 0) {
//         if (errno == EINTR) return; // Interrupted by signal, just return
//         throw std::runtime_error("poll() failed");
//     }
//     if (ret == 0) return; // Timeout, nothing to do

//     // Handle events (simplified, expand as needed)
//     for (size_t i = 0; i < fds.size(); ++i) {
// 		std::cout << "[DEBUG] we have sth to do "  << std::endl;

//         if (fds[i].revents & POLLIN) {
//             handle_client_data(i);
//             break; // fds may be modified, so break and re-poll
//         }
//     }
// }

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
    char buf[1024];

    std::cout << "[DEBUG] handle_client_data: FD=" << client_fd << std::endl;

    // Append new data to buffer
    std::string& request_data = client_buffers[client_fd];
    int bytes = read(client_fd, buf, sizeof(buf) - 1);

    if (bytes <= 0) {
        std::cout << "[DEBUG] Client disconnected or read error on FD=" << client_fd << std::endl;
        cleanup_client(client_fd, i);
        return;
    }

    request_data.append(buf, bytes);
    std::cout << "[DEBUG] request_data size after append: " << request_data.size() << std::endl;

    // Look for end of headers
    size_t header_end = request_data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        std::cout << "[DEBUG] Incomplete headers. Waiting for more data..." << std::endl;
        return;
    }

    std::string headers = request_data.substr(0, header_end);
    size_t body_start = header_end + 4;
    size_t body_length_received = request_data.size() - body_start;

    // Try to parse Content-Length if available
    int content_length = 0;
    {
        std::istringstream stream(headers);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("Content-Length:") != std::string::npos) {
                std::istringstream linestream(line);
                std::string key;
                linestream >> key >> content_length;
                break;
            }
        }
    }

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
		std::cout << "allowed methods are :" << loc->allowed_methods[0] << " and " << loc->allowed_methods[1]  << " and " << loc->allowed_methods[2]  << "\n";
        std::cout << "[DEBUG] Request: " << method << " " << uri << " from FD=" << client_fd << std::endl;

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
            send_error_response(client_fd, 405, "Method Not Allowed", i);
            cleanup_client(client_fd, i);
            return;
        }

        // 🔧 CGI Handling
		if (loc && is_cgi_request(*loc, uri)) {
			std::string script_path = resolve_script_path(uri, *loc);

			std::map<std::string, std::string> env;
			env["REQUEST_METHOD"] = method;

			size_t q = uri.find('?');
			std::string script_name = q == std::string::npos ? uri : uri.substr(0, q);
			std::string query_string = q == std::string::npos ? "" : uri.substr(q + 1);

			env["SCRIPT_NAME"] = script_name;
			env["QUERY_STRING"] = query_string;

			if (method == "POST") {
				std::ostringstream oss;
				oss << request.getBody().size();
				env["CONTENT_LENGTH"] = oss.str();
				env["CONTENT_TYPE"] = request.getHeader("Content-Type"); // optional default fallback
			}

			// Recommended CGI fields
			env["GATEWAY_INTERFACE"] = "CGI/1.1";
			env["SERVER_PROTOCOL"] = "HTTP/1.1";
			env["SERVER_SOFTWARE"] = "Webserv/1.0";
			env["REDIRECT_STATUS"] = "200";

			CGIHandler handler(script_path, env, request.getBody());
			std::string cgi_output = handler.execute();

			if (cgi_output == "__CGI_TIMEOUT__") {
				send_error_response(client_fd, 504, "Gateway Timeout", i);
			} else if (cgi_output == "__CGI_MISSING_HEADER__") {
				send_error_response(client_fd, 500, "Internal Server Error", i);
			} else {
				// Find the end of the CGI headers (should be "\r\n\r\n" or "\n\n")
				size_t header_end = cgi_output.find("\r\n\r\n");
				if (header_end == std::string::npos)
					header_end = cgi_output.find("\n\n");
				if (header_end == std::string::npos) {
					send_error_response(client_fd, 500, "CGI Output Missing Header", i);
					cleanup_client(client_fd, i);
					return;
				}

				std::string headers = cgi_output.substr(0, header_end);
				std::string body = cgi_output.substr(header_end + (cgi_output[header_end] == '\r' ? 4 : 2));

				// Check for Content-Type in CGI output
				if (headers.find("Content-Type:") == std::string::npos) {
					send_error_response(client_fd, 500, "CGI Output Missing Content-Type", i);
					cleanup_client(client_fd, i);
					return;
				}

				// Compose HTTP/1.1 response
				std::ostringstream oss;
				oss << "HTTP/1.1 200 OK\r\n"
					<< headers << "\r\n"
					<< "Content-Length: " << body.size() << "\r\n"
					<< "Connection: close\r\n\r\n"
					<< body;

				std::string response = oss.str();
				write(client_fd, response.c_str(), response.size());
			}

			cleanup_client(client_fd, i);
			return;
		}


        // 📤 Upload Handling
		if(method == "POST")
		{
			if (handle_upload(request, loc, client_fd, i)) {
				cleanup_client(client_fd, i);
				return;
			}
		}

        // ❌ DELETE Method
        if (method == "DELETE") {
            std::string path = resolve_path(uri, method);
            if (!file_exists(path)) {
                send_error_response(client_fd, 404, "Not Found", i);
            } else if (access(path.c_str(), W_OK) != 0) {
                send_error_response(client_fd, 403, "Forbidden", i);
            } else if (remove(path.c_str()) == 0) {
                std::string body = "<html><body><h1>File deleted: " + uri + "</h1></body></html>";
                std::ostringstream oss;
                oss << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: text/html\r\n"
                    << "Content-Length: " << body.size() << "\r\n"
                    << "Connection: close\r\n\r\n"
                    << body;
                write(client_fd, oss.str().c_str(), oss.str().size());
            } else {
                send_error_response(client_fd, 500, "Internal Server Error", i);
            }

            cleanup_client(client_fd, i);
            return;
        }

        // 📄 Serve Static File
        std::string path = resolve_path(uri, method);
        if (!file_exists(path)) {
            send_error_response(client_fd, 404, "Not Found", i);
        } else if (access(path.c_str(), R_OK) != 0) {
            send_error_response(client_fd, 403, "Forbidden", i);
        } else {
            send_response(client_fd, uri, method);
        }

        cleanup_client(client_fd, i);
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Request parsing failed: " << e.what() << std::endl;
        send_error_response(client_fd, 400, "Bad Request", i);
        cleanup_client(client_fd, i);
    }

    // Cleanup buffer after processing
    client_buffers.erase(client_fd);
}

// void WebServer::handle_client_data(size_t i) {
//     int client_fd = fds[i].fd;
//     char buf[1024];

//     std::cout << "[DEBUG] handle_client_data: FD=" << client_fd << std::endl;

//     // Use the persistent buffer for this client
//     std::string& request_data = client_buffers[client_fd];

//     int bytes = read(client_fd, buf, sizeof(buf) - 1);
//     std::cout << "[DEBUG] read() returned " << bytes << " bytes" << std::endl;

//     if (bytes <= 0) {
//         std::cout << "[DEBUG] Client disconnected or error on FD=" << client_fd << std::endl;
//         cleanup_client(client_fd, i);
//         return;
//     }

//     request_data.append(buf, bytes);
//     std::cout << "[DEBUG] request_data size after append: " << request_data.size() << std::endl;

//     // Check for complete headers
//     size_t header_end = request_data.find("\r\n\r\n");
//     std::cout << "[DEBUG] header_end: " << header_end << std::endl;

//     if (header_end == std::string::npos) {
//         std::cout << "[DEBUG] Headers not complete yet, waiting for more data." << std::endl;
//         return;  // ✅ Wait for more data
//     }

//     // If Content-Length is present, ensure full body is received
//     std::string headers = request_data.substr(0, header_end);
//     size_t cl_pos = headers.find("Content-Length:");
//     std::cout << "[DEBUG] Content-Length position: " << cl_pos << std::endl;

//     if (cl_pos != std::string::npos) {
//         std::istringstream iss(headers.substr(cl_pos));
//         std::string cl;
//         int content_length = 0;
//         iss >> cl >> content_length;

//         size_t body_start = header_end + 4;
//         size_t body_len = request_data.size() - body_start;
//         std::cout << "[DEBUG] body_len: " << body_len << ", content_length: " << content_length << std::endl;

//         if (body_len < (size_t)content_length) {
//             std::cout << "[DEBUG] Body not complete yet, waiting for more data." << std::endl;
//             return;  // ✅ Wait for more data
//         }
//     }

//     std::cout << "[DEBUG] Full request received, processing..." << std::endl;

//     try {
//         Request request = Request(request_data);
//         std::string uri = request.getPath();
//         const LocationConfig* loc = match_location(g_config.getLocations(), uri);
//         std::string method = request.getMethod();

//         std::cout << "[DEBUG] Request: " << method << " " << uri << " from FD=" << client_fd << std::endl;

//         std::set<std::string> supported_methods;
//         supported_methods.insert("GET");
//         supported_methods.insert("POST");
//         supported_methods.insert("DELETE");
//         if (supported_methods.find(method) == supported_methods.end()) {
//             send_error_response(client_fd, 501, "Not Implemented", i);
//             cleanup_client(client_fd, i);
//             return;
//         }

//         size_t max_body_size = g_config.getMaxBodySize();
//         if (request.getBody().size() > max_body_size) {
//             send_error_response(client_fd, 413, "Payload Too Large", i);
//             cleanup_client(client_fd, i);
//             return;
//         }

//         if (loc) {
//             const std::vector<std::string>& allowed = loc->allowed_methods;
//             if (std::find(allowed.begin(), allowed.end(), method) == allowed.end()) {
//                 send_error_response(client_fd, 405, "Method Not Allowed", i);
//                 cleanup_client(client_fd, i);
//                 return;
//             }
//         }

//         // 🔧 Handle CGI
//         if (loc && is_cgi_request(*loc, uri)) {
//             std::string script_path = resolve_script_path(uri, *loc);

//             std::map<std::string, std::string> env;
//             env["REQUEST_METHOD"] = method;
//             env["SCRIPT_NAME"] = uri;
//             env["QUERY_STRING"] = "";
//             std::ostringstream oss;
//             oss << request.getBody().size();
//             env["CONTENT_LENGTH"] = oss.str();

//             CGIHandler handler(script_path, env, request.getBody());
//             std::string cgi_output = handler.execute();

//             if (cgi_output == "__CGI_TIMEOUT__") {
//                 send_error_response(client_fd, 504, "Gateway Timeout", i);
//                 cleanup_client(client_fd, i);
//                 return;
//             }
//             if (cgi_output == "__CGI_MISSING_HEADER__") {
//                 send_error_response(client_fd, 500, "Internal Server Error", i);
//                 cleanup_client(client_fd, i);
//                 return;
//             }

//             write(client_fd, cgi_output.c_str(), cgi_output.size());
//             cleanup_client(client_fd, i);
//             return;
//         }

//         // 📝 Handle Upload
//         if (handle_upload(request, loc, client_fd, i)) {
//             cleanup_client(client_fd, i);
//             return;
//         }

//         // 🗑️ Handle DELETE
//         if (method == "DELETE") {
//             std::string path = resolve_path(uri);
//             if (!file_exists(path)) {
//                 send_error_response(client_fd, 404, "Not Found", i);
//                 cleanup_client(client_fd, i);
//                 return;
//             }
//             if (access(path.c_str(), W_OK) != 0) {
//                 send_error_response(client_fd, 403, "Forbidden", i);
//                 cleanup_client(client_fd, i);
//                 return;
//             }
//             if (remove(path.c_str()) == 0) {
//                 std::string body = "<html><body><h1>File deleted: " + uri + "</h1></body></html>";
//                 std::ostringstream oss;
//                 oss << "HTTP/1.1 200 OK\r\n"
//                     << "Content-Type: text/html\r\n"
//                     << "Content-Length: " << body.size() << "\r\n"
//                     << "Connection: close\r\n\r\n"
//                     << body;
//                 write(client_fd, oss.str().c_str(), oss.str().size());
//             } else {
//                 send_error_response(client_fd, 500, "Internal Server Error", i);
//             }
//             cleanup_client(client_fd, i);
//             return;
//         }

//         // 📄 Serve Static File
//         std::string path = resolve_path(uri);
//         if (!file_exists(path)) {
//             send_error_response(client_fd, 404, "Not Found", i);
//             cleanup_client(client_fd, i);
//             return;
//         }
//         if (access(path.c_str(), R_OK) != 0) {
//             send_error_response(client_fd, 403, "Forbidden", i);
//             cleanup_client(client_fd, i);
//             return;
//         }

//         send_response(client_fd, uri, method);
//         cleanup_client(client_fd, i);
//     }
//     catch (const std::exception& e) {
//         std::cerr << "Request parse error: " << e.what() << "\n";
//         send_error_response(client_fd, 400, "Bad Request", i);
//         cleanup_client(client_fd, i);
//     }

//     // ❌ Removed: client_buffers.erase(client_fd);
//     // ✅ Let cleanup_client handle buffer cleanup if needed
// }


void WebServer::send_response(int client_fd, const std::string& raw_path, const std::string& method) {
    std::string file_path = resolve_path(raw_path, method);
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
void WebServer::send_error_response(int client_fd, int code, const std::string& reason, size_t i) {
    std::string body;
    std::string custom_path = get_custom_error_page_path(code); // Implement this
    if (!custom_path.empty() && file_exists(custom_path)) {
        body = read_file(custom_path); // Implement this
    } else {
        body = get_default_error_page(code);
    }
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << reason << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    write(client_fd, oss.str().c_str(), oss.str().size());
    close(client_fd);
    fds.erase(fds.begin() + i);
}

std::string WebServer::resolve_path(const std::string& raw_path, const std::string& method) {
    std::string path = raw_path;
    std::string base_dir = "./www";
    std::string resolved_path;

    std::cout << "[DEBUG] resolve_path: raw_path = \"" << raw_path << "\", method = " << method << std::endl;

    // Redirect root requests based on method
    if (path == "/") {
        if (method == "POST" || method == "DELETE") {
            path = "/upload";
        } else {
            path = "/html/index.html";
        }
        std::cout << "[DEBUG] resolve_path: root path adjusted to \"" << path << "\"" << std::endl;
    }

    // Append index.html to paths ending with '/'
    if (!path.empty() && path[path.size() - 1] == '/') {
        path += "html/index.html";
        std::cout << "[DEBUG] resolve_path: trailing slash - path set to \"" << path << "\"" << std::endl;
    }

    // Construct full path to file
    resolved_path = base_dir + path;
    std::cout << "[DEBUG] resolve_path: checking resolved_path = \"" << resolved_path << "\"" << std::endl;

    if (file_exists(resolved_path)) {
        std::cout << "[DEBUG] resolve_path: file found - returning \"" << resolved_path << "\"" << std::endl;
        return resolved_path;
    }

    // Try appending .html if no file extension is present
    if (path.find('.') == std::string::npos) {
        std::string fallback_path = base_dir + path + ".html";
        std::cout << "[DEBUG] resolve_path: no extension, checking fallback = \"" << fallback_path << "\"" << std::endl;

        if (file_exists(fallback_path)) {
            std::cout << "[DEBUG] resolve_path: fallback found - returning \"" << fallback_path << "\"" << std::endl;
            return fallback_path;
        }
    }

    std::cout << "[DEBUG] resolve_path: file not found - returning original path \"" << resolved_path << "\"" << std::endl;
    return resolved_path;
}

std::string extract_filename(const std::string& header) {
    size_t pos = header.find("filename=");
    if (pos == std::string::npos)
        return "upload";

    size_t start = header.find('"', pos);
    size_t end = header.find('"', start + 1);
    if (start == std::string::npos || end == std::string::npos)
        return "upload";

    return header.substr(start + 1, end - start - 1);
}

std::string extract_file_from_multipart(const std::string& body, std::string& filename) {
    std::istringstream stream(body);
    std::string line;
    bool in_headers = true;
    bool in_content = false;
    std::ostringstream file_content;

    filename = "upload";  // default fallback

    while (std::getline(stream, line)) {
        // Remove \r from the end if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        // Boundary or end
        if (line.find("------") == 0) {
            if (in_content) break;
            continue;
        }

        // Extract filename
        if (line.find("Content-Disposition:") != std::string::npos) {
            size_t pos = line.find("filename=");
            if (pos != std::string::npos) {
                size_t start = line.find('"', pos);
                size_t end = line.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    filename = line.substr(start + 1, end - start - 1);
                }
            }
        }

        // Headers done, content starts after blank line
        if (line.empty() && in_headers) {
            in_headers = false;
            in_content = true;
            continue;
        }

        if (in_content)
            file_content << line << "\n";
    }

    // Remove last newline
    std::string content = file_content.str();
    if (!content.empty() && content[content.size() - 1] == '\n')
        content.erase(content.size() - 1);

    return content;
}

std::string WebServer::timestamp() {
    time_t now = time(NULL);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    return std::string(buf);
}
bool WebServer::handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i) {
    if (request.getMethod() != "POST" || !loc || loc->upload_dir.empty())
	{
		std::cout << "🔍 now a upload:\n";
        return false;
	}

	std::cout << "🔍 Raw multipart body:\n" << request.getBody() << "\n--- END ---\n";

    std::string filename;
    std::string content = extract_file_from_multipart(request.getBody(), filename);

    // Clean filename (optional: strip path, extensions, etc.)
    size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos)
        filename = filename.substr(slash + 1);

    // Append timestamp
    std::string full_filename = filename + "_" + timestamp() + ".txt";
    std::string full_path = loc->upload_dir + "/" + full_filename;

    std::ofstream out(full_path.c_str(), std::ios::binary);
    if (!out) {
        std::cerr << "❌ Failed to open file: " << full_path << "\n";
        send_error_response(client_fd, 500, "Failed to save upload", i);
        return true;
    }

    out << content;
    out.close();

    Response res;
    res.setStatus(200, "OK");
    res.setBody("<h1>✅ File uploaded as " + full_filename + "</h1>");
    std::string raw = res.toString();
    write(client_fd, raw.c_str(), raw.size());

    close(client_fd);
    fds.erase(fds.begin() + i);
    return true;
}

// Returns the path to a custom error page for a given code, or empty string if not set
std::string WebServer::get_custom_error_page_path(int code) {
    // Example: check config for custom error page
    std::map<int, std::string>::const_iterator it = g_config.getErrorPages().find(code);
    if (it != g_config.getErrorPages().end())
        return it->second;
    return "";
}

// Reads the entire contents of a file into a string
std::string WebServer::read_file(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Returns a default HTML error page for the given code
std::string WebServer::get_default_error_page(int code) {
    switch (code) {
        case 400:
            return "<html><head><title>400 Bad Request</title></head>"
                   "<body><h1>400 Bad Request</h1><p>Your browser sent a request that this server could not understand.</p></body></html>";
        case 401:
            return "<html><head><title>401 Unauthorized</title></head>"
                   "<body><h1>401 Unauthorized</h1><p>Authentication is required to access this resource.</p></body></html>";
        case 403:
            return "<html><head><title>403 Forbidden</title></head>"
                   "<body><h1>403 Forbidden</h1><p>You don't have permission to access this resource.</p></body></html>";
        case 404:
            return "<html><head><title>404 Not Found</title></head>"
                   "<body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
        case 405:
            return "<html><head><title>405 Method Not Allowed</title></head>"
                   "<body><h1>405 Method Not Allowed</h1><p>The method is not allowed for the requested URL.</p></body></html>";
        case 408:
            return "<html><head><title>408 Request Timeout</title></head>"
                   "<body><h1>408 Request Timeout</h1><p>The server timed out waiting for the request.</p></body></html>";
        case 413:
            return "<html><head><title>413 Payload Too Large</title></head>"
                   "<body><h1>413 Payload Too Large</h1><p>The request is too large for this server to process.</p></body></html>";
        case 500:
            return "<html><head><title>500 Internal Server Error</title></head>"
                   "<body><h1>500 Internal Server Error</h1><p>The server encountered an internal error.</p></body></html>";
        case 501:
            return "<html><head><title>501 Not Implemented</title></head>"
                   "<body><h1>501 Not Implemented</h1><p>The server does not support the functionality required to fulfill the request.</p></body></html>";
        case 502:
            return "<html><head><title>502 Bad Gateway</title></head>"
                   "<body><h1>502 Bad Gateway</h1><p>The server received an invalid response from the upstream server.</p></body></html>";
        case 503:
            return "<html><head><title>503 Service Unavailable</title></head>"
                   "<body><h1>503 Service Unavailable</h1><p>The server is currently unable to handle the request due to temporary overload or maintenance.</p></body></html>";
        case 504:
            return "<html><head><title>504 Gateway Timeout</title></head>"
                   "<body><h1>504 Gateway Timeout</h1><p>The server did not receive a timely response from the upstream server.</p></body></html>";
        default:
            return "<html><head><title>Error</title></head>"
                   "<body><h1>An error occurred</h1></body></html>";
    }
}



