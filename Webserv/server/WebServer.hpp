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
    int server_fd;
    std::vector<pollfd> fds;
    int port; 

    void setup_server_socket(int port);
    void make_socket_non_blocking(int fd);
    void poll_loop();
    void handle_new_connection();
    void handle_client_data(size_t i);
    void send_response(int client_fd, const std::string& path);
    std::string resolve_path(const std::string& raw_path);
};


#endif
