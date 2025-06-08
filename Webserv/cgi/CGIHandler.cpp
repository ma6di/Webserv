/**
 * CGIHandler.cpp
 * --------------
 * Implements the CGIHandler class.
 * - Prepares environment and arguments for CGI scripts
 * - Handles pipes for stdin, stdout, and stderr
 * - Manages child process execution
 * - Captures and validates CGI output
 */

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

std::string CGIHandler::runCGI() {
    // Absolute path resolution
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    std::string absPath = std::string(cwd) + "/" + scriptPath;

    int input_pipe[2], output_pipe[2], error_pipe[2];
    if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0 || pipe(error_pipe) < 0)
        throw std::runtime_error("Pipe creation failed");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("Fork failed");

    if (pid == 0) {
        std::cerr << "[CGI] Forked child to execute: " << absPath << std::endl;

        // Redirect stdin
        if (dup2(input_pipe[0], STDIN_FILENO) == -1) {
            perror("[CGI] dup2 STDIN failed");
            exit(1);
        }
        std::cerr << "[CGI] dup2 STDIN succeeded" << std::endl;
        close(input_pipe[0]);
        close(input_pipe[1]);
        std::cerr << "[CGI] closed input_pipe" << std::endl;

        // Redirect stdout
        if (dup2(output_pipe[1], STDOUT_FILENO) == -1) {
            perror("[CGI] dup2 STDOUT failed");
            exit(1);
        }
        std::cerr << "[CGI] dup2 STDOUT succeeded" << std::endl;
        close(output_pipe[0]);
        close(output_pipe[1]);
        std::cerr << "[CGI] closed output_pipe" << std::endl;

        // Redirect stderr
        if (dup2(error_pipe[1], STDERR_FILENO) == -1) {
            perror("[CGI] dup2 STDERR failed");
            exit(1);
        }
        std::cerr << "[CGI] dup2 STDERR succeeded" << std::endl;
        close(error_pipe[0]);
        close(error_pipe[1]);
        std::cerr << "[CGI] closed error_pipe" << std::endl;

        // Prepare envp
        std::vector<std::string> envStrings;
        for (std::map<std::string, std::string>::const_iterator it = environment.begin(); it != environment.end(); ++it)
            envStrings.push_back(it->first + "=" + it->second);

        std::vector<char*> envp;
        for (size_t i = 0; i < envStrings.size(); ++i)
            envp.push_back(const_cast<char*>(envStrings[i].c_str()));
        envp.push_back(NULL);

        // Prepare argv
        char* argv[] = { const_cast<char*>(absPath.c_str()), NULL };

        std::cerr << "[CGI] About to execve: " << absPath << std::endl;
        execve(absPath.c_str(), argv, envp.data());

        // If execve fails
        perror("[CGI] execve failed");
        exit(1);
    }

    // --- PARENT PROCESS ---
    close(input_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[1]);

    // Send request body (e.g., POST) to CGI stdin
    if (!inputBody.empty()) {
        std::cerr << "[CGI] Writing inputBody to CGI stdin" << std::endl;
        write(input_pipe[1], inputBody.c_str(), inputBody.size());
    }
    close(input_pipe[1]);

    // Read CGI stdout
    std::string output;
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = read(output_pipe[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, bytesRead);
    close(output_pipe[0]);

    // Read CGI stderr
    std::string error_output;
    while ((bytesRead = read(error_pipe[0], buffer, sizeof(buffer))) > 0)
        error_output.append(buffer, bytesRead);
    close(error_pipe[0]);

    // Wait for child process
    int status;
    waitpid(pid, &status, 0);

    std::cerr << "[CGI] Child exited, status: " << status << std::endl;
    if (WIFSIGNALED(status)) {
        std::cerr << "[CGI] CGI script killed by signal: " << WTERMSIG(status) << std::endl;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "[CGI] CGI script exited abnormally" << std::endl;
        if (!error_output.empty())
            std::cerr << "[CGI] CGI script stderr: " << error_output << std::endl;
        else
            std::cerr << "[CGI] CGI script stderr was empty" << std::endl;
        return "__CGI_INTERNAL_ERROR__";
    }

    // Validate headers
    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos)
        header_end = output.find("\n\n");

    bool has_content_type = false;
    if (header_end != std::string::npos) {
        std::string headers = output.substr(0, header_end);
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.find("Content-Type:") == 0 || line.find("content-type:") == 0) {
                has_content_type = true;
                break;
            }
        }
    }

    if (!has_content_type)
        return "__CGI_MISSING_HEADER__";

    return output;
}
