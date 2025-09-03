#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>  // for time_t and time()
#include <netinet/in.h>  // sockaddr_in
#include <netdb.h>      // gethostbyname
#include <arpa/inet.h>  // inet_aton, htons
#include <cstring>  
#include "../logger/Logger.hpp"
#include "Config.hpp"
#include "LocationConfig.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "utils.hpp"

struct Connection {
    std::string readBuf;
    std::string writeBuf;
    bool        shouldCloseAfterWrite;
    time_t      last_active;
    pid_t       cgi_pid;
    int         cgi_stdin_fd[2];
    int         cgi_stdout_fd[2];
    bool        cgi_active;
    std::string cgi_input_buffer;
    std::string cgi_output_buffer;

    Connection()
        : readBuf(), writeBuf(), shouldCloseAfterWrite(false), last_active(time(NULL)),
          cgi_pid(-1), cgi_active(false)
    {
        cgi_stdin_fd[0] = -1;
        cgi_stdin_fd[1] = -1;
        cgi_stdout_fd[0] = -1;
        cgi_stdout_fd[1] = -1;
    }
};

