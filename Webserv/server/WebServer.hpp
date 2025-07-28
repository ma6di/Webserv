#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <sys/stat.h>
#include "../logger/Logger.hpp"
#include "utils.hpp"
#include "CGIHandler.hpp"
#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <ctime>
#include <map>

class WebServer {
public:
    WebServer(const std::vector<int>& ports);
    std::vector<int> listening_sockets;
    ~WebServer();
    void run();
    void run_one_iteration();
    void shutdown();

    std::string read_file(const std::string& path);
    std::string get_default_error_page(int code);

private:
    int server_fd;
    std::vector<pollfd> fds;
    std::map<int, std::string> client_buffers;

    void setup_server_socket(int port);
    void make_socket_non_blocking(int fd);
    void poll_loop();
    void handle_new_connection(int listen_fd);
    void handle_client_data(size_t i);
    std::string resolve_path(const std::string& raw_path, const std::string& method, const LocationConfig* loc);
    void cleanup_client(int fd, size_t i);

    // --- HTTP Method Handlers ---
    void handle_get(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    void handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    void handle_delete(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    void handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i);

    // --- Directory and File Helpers ---
    void handle_directory_request(const std::string& path, const std::string& uri, const LocationConfig* loc, int client_fd, size_t i);
    void handle_file_request(const std::string& path, int client_fd, size_t i);

    // --- Upload Helpers ---
    bool handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    bool is_valid_upload_request(const Request& request, const LocationConfig* loc);
    void process_upload_content(const Request& request, std::string& filename, std::string& content);
    std::string make_upload_filename(const std::string& filename);
    bool write_upload_file(const std::string& full_path, const std::string& content);
    static std::string timestamp();
    void send_upload_success_response(int client_fd, const std::string& full_filename, size_t i);

    // --- Response Helpers ---
    void send_ok_response(int client_fd, const std::string& body, const std::map<std::string, std::string>& headers, size_t i);
    void send_error_response(int client_fd, int code, const std::string& msg, size_t i);
    void send_file_response(int client_fd, const std::string& path, size_t i);
    void send_redirect_response(int client_fd, int code, const std::string& location, size_t i);

	size_t find_header_end(const std::string& request_data);
	bool read_and_append_client_data(int client_fd, size_t i);
	int parse_content_length(const std::string& headers);
	bool is_full_body_received(const Request& request, const std::string& request_data, size_t header_end);
	void process_request(Request& request, int client_fd, size_t i);


};

std::map<std::string, std::string> single_header(const std::string& k, const std::string& v);
std::string extract_file_from_multipart(const std::string& body, std::string& filename);

inline std::string to_str(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

#endif
