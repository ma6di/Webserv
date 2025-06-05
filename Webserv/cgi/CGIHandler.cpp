#include "CGIHandler.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <iostream>

CGIHandler::CGIHandler(const std::string& scriptPath,
                       const std::map<std::string, std::string>& env,
                       const std::string& inputBody)
    : scriptPath(scriptPath), environment(env), inputBody(inputBody) {}

std::string CGIHandler::execute() {
    return runCGI();
}

// ...existing code...
std::string CGIHandler::runCGI() {
    int input_pipe[2];
    int output_pipe[2];
    int error_pipe[2]; // Add pipe for stderr

    if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0 || pipe(error_pipe) < 0)
        throw std::runtime_error("Pipe creation failed");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("Fork failed");

    if (pid == 0) {
        // Child process

		// Set a timeout of 5 seconds for the CGI script
		alarm(5);

        // Redirect stdin from input_pipe
        dup2(input_pipe[0], STDIN_FILENO);
        close(input_pipe[1]);
        close(input_pipe[0]);

        // Redirect stdout to output_pipe
        dup2(output_pipe[1], STDOUT_FILENO);
        close(output_pipe[0]);
        close(output_pipe[1]);

        // Redirect stderr to error_pipe
        dup2(error_pipe[1], STDERR_FILENO);
        close(error_pipe[0]);
        close(error_pipe[1]);

        // Build envp
        std::vector<std::string> envStrings;
        for (std::map<std::string, std::string>::const_iterator it = environment.begin(); it != environment.end(); ++it)
            envStrings.push_back(it->first + "=" + it->second);

        std::vector<char*> envp;
        for (size_t i = 0; i < envStrings.size(); ++i)
            envp.push_back(const_cast<char*>(envStrings[i].c_str()));
        envp.push_back(NULL);

        // Build argv
        char* argv[] = {
            const_cast<char*>(scriptPath.c_str()),
            NULL
        };

        execve(scriptPath.c_str(), argv, envp.data());
        exit(1); // execve failed
    }

    // Parent process
    close(input_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[1]);

    // Write input body (for POST)
    if (!inputBody.empty())
        write(input_pipe[1], inputBody.c_str(), inputBody.size());
    close(input_pipe[1]);

    // Read CGI output (stdout)
    std::string output;
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, bytesRead);
    }
    close(output_pipe[0]);

    // Read CGI error output (stderr)
    std::string error_output;
    while ((bytesRead = read(error_pipe[0], buffer, sizeof(buffer))) > 0) {
        error_output.append(buffer, bytesRead);
    }
    close(error_pipe[0]);

    // Wait for child
    int status;
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        std::cerr << "CGI script timed out" << std::endl;
        return "__CGI_TIMEOUT__";
    }
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		std::cerr << "CGI script exited abnormally" << std::endl;
		return "__CGI_INTERNAL_ERROR__";
	}

    // Optionally, log or handle error_output as needed
    if (!error_output.empty()) {
        // For now, print to server's stderr
        std::cerr << "CGI script stderr: " << error_output << std::endl;
    }

	// --- Header Validation ---
    // Find end of headers (double CRLF or double LF)
    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos)
        header_end = output.find("\n\n");
    bool has_content_type = false;
    if (header_end != std::string::npos) {
        std::string headers = output.substr(0, header_end);
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line)) {
            // Remove trailing \r if present
            if (!line.empty() && line[line.size()-1] == '\r')
                line.erase(line.size()-1);
            if (line.find("Content-Type:") == 0 || line.find("content-type:") == 0) {
                has_content_type = true;
                break;
            }
        }
    }

    if (!has_content_type) {
        // If missing, return a 500 error response
		return "__CGI_MISSING_HEADER__";
    }
    return output;
}
// ...existing code...
