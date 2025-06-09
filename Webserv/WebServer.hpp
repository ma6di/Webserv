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
#include <dirent.h>
#include "Router.hpp"
#include "Request.hpp"
#include "Response.hpp"

class WebServer {
public:
    WebServer(int port);
    ~WebServer();
    void run();

private:
    int server_fd;
    std::vector<pollfd> fds;
    int port; 

    Router router;
    void setup_routes();
    void setup_server_socket(int port);
    void make_socket_non_blocking(int fd);
    void poll_loop();
    void handle_new_connection();
    void handle_client_data(size_t i);
    void send_response(int client_fd, const Request& path);
    std::string join_methods(const std::vector<std::string>& methods) const;
    void finalize_response_and_send(int client_fd, Response& res);
    std::string resolve_final_path(const std::string& path, const Route* route);
};

#endif