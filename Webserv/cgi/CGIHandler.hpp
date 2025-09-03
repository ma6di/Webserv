#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <sys/time.h>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iostream>
#include "Request.hpp"
#include "../logger/Logger.hpp"
#include "WebServer.hpp"
#include "Connection.hpp"




// The CGIHandler class is responsible for executing CGI scripts like .py or .php files.
// It passes environment variables and (optional) input data to the script,
// captures its output, and returns it back as a response string.
class CGIHandler {
public:
    // Constructor takes the path to the script, the environment variables, and optional POST data
    //scriptPath: full path to the script to execute (e.g., /www/cgi/test.py)
CGIHandler(const std::string& scriptPath,
           const std::map<std::string, std::string>& env,
           Connection* conn,
           const std::string& inputBody,
           const std::string& requestedUri);

    // Executes the CGI program and returns its output (headers + body)
    std::string execute();

    static bool find_cgi_script(const std::string& cgi_root, const std::string& cgi_uri, const std::string& uri,
                                std::string& script_path, std::string& script_name, std::string& path_info);

    static std::map<std::string, std::string> build_cgi_env(const Request& request,
                                                            const std::string& script_name,
                                                            const std::string& path_info);

    static void parse_cgi_output(const std::string& cgi_output, std::map<std::string, std::string>& cgi_headers, std::string& body);
private:
	std::string scriptPath;
	std::map<std::string, std::string> environment;
	Connection* conn;
	std::string inputBody;
	std::string requestedUri;

    // Internal method that handles process creation, piping, and reading output
    std::string runCGI(); 

    std::string resolve_script_path() const;
    bool create_pipes(int input_pipe[2], int output_pipe[2], int error_pipe[2]) const;
    void setup_child_process(const std::string& absPath, int input_pipe[2], int output_pipe[2], int error_pipe[2]);
    void send_input_to_cgi(int input_fd) const;
    std::string read_from_pipe(int fd) const;
    bool check_child_status(int status, const std::string& error_output) const;
    bool validate_cgi_headers(const std::string& output) const;
	int wait_for_child_with_timeout(pid_t pid, int& status, bool& timed_out);
	void handle_timeout(pid_t pid, int& status);
	std::string read_pipe_to_string(int fd) const;
	void log_cgi_debug(int status, int ret, const std::string& output, const std::string& error_output) const;
};

#endif
