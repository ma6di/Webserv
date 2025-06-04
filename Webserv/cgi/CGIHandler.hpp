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
            	const std::string& inputBody = "");

    // Executes the CGI program and returns its output (headers + body)
    std::string execute();

private:
    std::string scriptPath;                     // Path to the CGI script (e.g., ./cgi-bin/test.py)
    std::map<std::string, std::string> environment; // Environment variables for the CGI execution
    std::string inputBody;                      // Input body for POST requests (sent to stdin)

    // Internal method that handles process creation, piping, and reading output
    std::string runCGI(); 
};

#endif
