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
#include "WebServer.hpp"
#include "utils.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <unistd.h>
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

    std::string get_custom_error_page_path(int code);
    std::string read_file(const std::string& path);
    std::string get_default_error_page(int code);

private:
	//server_fd: the file descriptor of the main server socket.
    int server_fd;
	//fds: list of pollfd objects (one per client or server socket).
    std::vector<pollfd> fds;
	//port: the port number the server listens on.
    int port; 
    std::map<int, std::string> client_buffers; // Add this line

    void setup_server_socket(int port);
    void make_socket_non_blocking(int fd);
    void poll_loop();
    void handle_new_connection(int listen_fd);
    void handle_client_data(size_t i);
	void send_response(int client_fd, const std::string& raw_path, const std::string& method);
	std::string resolve_path(const std::string& raw_path, const std::string& method);
	void send_error_response(int client_fd, int status_code, const std::string& status_text, size_t index);
	bool handle_upload(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
	static std::string timestamp();
	void cleanup_client(int fd, size_t i);
    bool is_valid_upload_request(const Request& request, const LocationConfig* loc);
    void process_upload_content(const Request& request, std::string& filename, std::string& content);
    std::string make_upload_filename(const std::string& filename);
    bool write_upload_file(const std::string& full_path, const std::string& content);
    void send_upload_success_response(int client_fd, const std::string& full_filename, size_t i);
	private:
    void handle_get(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    void handle_post(const Request& request, const LocationConfig* loc, int client_fd, size_t i);
    void handle_delete(const Request& request, int client_fd, size_t i);
    bool read_and_append_client_data(int client_fd, size_t i);
    size_t find_header_end(const std::string& request_data);
    int parse_content_length(const std::string& headers);
	void handle_cgi(const LocationConfig* loc, const Request& request, int client_fd, size_t i);
    void send_redirect_response(int client_fd, int code, const std::string& location, size_t i);
};


#endif
