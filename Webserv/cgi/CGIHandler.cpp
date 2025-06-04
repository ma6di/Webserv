#include "CGIHandler.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <stdexcept>

CGIHandler::CGIHandler(const std::string& scriptPath,
                       const std::map<std::string, std::string>& env,
                       const std::string& inputBody)
    : scriptPath(scriptPath), environment(env), inputBody(inputBody) {}

std::string CGIHandler::execute() {
    return runCGI();
}

std::string CGIHandler::runCGI() {
    int input_pipe[2];
    int output_pipe[2];
	//Creates two pipes:
		//One for sending input to the script (stdin)
		//One for reading output from the script (stdout)
    if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0)
        throw std::runtime_error("Pipe creation failed");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("Fork failed");

    if (pid == 0) {
        // Child process

        // Redirect stdin from input_pipe
        dup2(input_pipe[0], STDIN_FILENO);
        close(input_pipe[1]);
        close(input_pipe[0]);

        // Redirect stdout to output_pipe
        dup2(output_pipe[1], STDOUT_FILENO);
        close(output_pipe[0]);
        close(output_pipe[1]);

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

    // Write input body (for POST)
    if (!inputBody.empty())
        write(input_pipe[1], inputBody.c_str(), inputBody.size());
    close(input_pipe[1]);

    // Read CGI output
    std::string output;
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, bytesRead);
    }
    close(output_pipe[0]);

    // Wait for child
    waitpid(pid, NULL, 0);

    return output;
}

