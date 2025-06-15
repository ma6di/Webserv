#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>

// The CGIHandler class is responsible for executing CGI scripts like .py or .php files.
// It passes environment variables and (optional) input data to the script,
// captures its output, and returns it back as a response string.
class CGIHandler {
public:
    // Constructor takes the path to the script, the environment variables, and optional POST data
    //scriptPath: full path to the script to execute (e.g., /www/cgi/test.py)
	CGIHandler(const std::string& scriptPath,
				//env: a map of environment variables
            	const std::map<std::string, std::string>& env,
				//inputBody: optional request body (for POST methods)
            	const std::string& inputBody = "",
			    const std::string& requestedUri = "");

    // Executes the CGI program and returns its output (headers + body)
    std::string execute();

private:
    std::string scriptPath;                     // Path to the CGI script (e.g., ./cgi-bin/test.py)
    std::map<std::string, std::string> environment; // Environment variables for the CGI execution
    std::string inputBody;   
	std::string requestedUri;
                   // Input body for POST requests (sent to stdin)

    // Internal method that handles process creation, piping, and reading output
    std::string runCGI(); 

    std::string resolve_script_path() const;
    bool create_pipes(int input_pipe[2], int output_pipe[2], int error_pipe[2]) const;
    void setup_child_process(const std::string& absPath, int input_pipe[2], int output_pipe[2], int error_pipe[2]);
    void send_input_to_cgi(int input_fd) const;
    std::string read_from_pipe(int fd) const;
    bool check_child_status(int status, const std::string& error_output) const;
    bool validate_cgi_headers(const std::string& output) const;
};

#endif
