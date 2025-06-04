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

class WebServer {
public:
    WebServer(int port);
    ~WebServer();
    void run();

private:
	//server_fd: the file descriptor of the main server socket.
    int server_fd;
	//fds: list of pollfd objects (one per client or server socket).
    std::vector<pollfd> fds;
	//port: the port number the server listens on.
    int port; 

    void setup_server_socket(int port);
    void make_socket_non_blocking(int fd);
    void poll_loop();
    void handle_new_connection();
    void handle_client_data(size_t i);
    void send_response(int client_fd, const std::string& path);
    std::string resolve_path(const std::string& raw_path);
	void send_error_response(int client_fd, int status_code, const std::string& status_text, size_t index);

};


#endif
