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
#include <cstdio>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/time.h>
#include <signal.h>

// --- Signal handler for alarm (timeout) ---
void cgi_alarm_handler(int) { /* do nothing */ }

CGIHandler::CGIHandler(const std::string& scriptPath,
                       const std::map<std::string, std::string>& env,
                       const std::string& inputBody)
    : scriptPath(scriptPath), environment(env), inputBody(inputBody) {}

std::string CGIHandler::execute() {
    return runCGI();
}

std::string CGIHandler::runCGI() {
    std::string absPath = resolve_script_path();
    std::cerr << "[CGI] Executing script: " << absPath << std::endl;
    int input_pipe[2], output_pipe[2], error_pipe[2];
    if (!create_pipes(input_pipe, output_pipe, error_pipe)) {
        std::cerr << "[CGI] Pipe creation failed\n";
        throw std::runtime_error("Pipe creation failed");
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[CGI] Fork failed\n";
        throw std::runtime_error("Fork failed");
    }

    if (pid == 0) {
        // --- CHILD ---
        setup_child_process(absPath, input_pipe, output_pipe, error_pipe);
    }

    std::cerr << "[CGI] forked child PID: " << pid << std::endl;

    // --- PARENT PROCESS ---
    close(input_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[1]);

    send_input_to_cgi(input_pipe[1]);
    close(input_pipe[1]);

    // --- Timeout logic ---
    int timeout_seconds = 5; // Set your desired timeout here

    // Block SIGALRM so only this thread handles it
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    struct sigaction sa;
    sa.sa_handler = cgi_alarm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    alarm(timeout_seconds);

    int status = 0;
    int ret = 0;
    bool timed_out = false;

    // Wait for child with timeout
    while (true) {
        ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) break;
        if (ret == -1) break;
        // Check if alarm fired
        sigset_t pending;
        sigpending(&pending);
        if (sigismember(&pending, SIGALRM)) {
            timed_out = true;
            break;
        }
        usleep(10000); // Sleep 10ms
    }

    alarm(0); // Cancel alarm
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    if (timed_out) {
        std::cerr << "[CGI] CGI script timed out, killing PID " << pid << std::endl;
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0); // Reap
        return "__CGI_TIMEOUT__";
    }

    // Read CGI output until EOF
    std::string output;
    char buffer[4096];
    ssize_t n;
    while ((n = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, n);
    }
    close(output_pipe[0]);

    std::string error_output = read_from_pipe(error_pipe[0]);
    close(error_pipe[0]);

    std::cerr << "[CGI] waitpid returned: " << ret << ", status: " << status << std::endl;
    std::cerr << "[CGI] WIFEXITED: " << WIFEXITED(status) << ", WEXITSTATUS: " << WEXITSTATUS(status) << std::endl;
    std::cerr << "[CGI] WIFSIGNALED: " << WIFSIGNALED(status) << ", WTERMSIG: " << WTERMSIG(status) << std::endl;

    std::cerr << "[CGI DEBUG] Raw CGI output (hex):\n";
    for (size_t i = 0; i < output.size(); ++i)
        std::cerr << std::hex << (int)(unsigned char)output[i] << " ";
    std::cerr << "\n[END HEX]\n";
    std::cerr << "[CGI DEBUG] CGI ERROR output:\n" << error_output << "\n[END]\n";

    if (!check_child_status(status, error_output))
        return "__CGI_INTERNAL_ERROR__";

    if (!validate_cgi_headers(output))
        return "__CGI_MISSING_HEADER__";

    return output;
}

// --- Helper methods ---

std::string CGIHandler::resolve_script_path() const {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("[CGI] getcwd failed");
        throw std::runtime_error("getcwd failed");
    }
    std::string abs = std::string(cwd) + "/" + scriptPath;
    std::cerr << "[CGI] Resolved script path: " << abs << std::endl;
    return abs;
}

bool CGIHandler::create_pipes(int input_pipe[2], int output_pipe[2], int error_pipe[2]) const {
    bool ok = (pipe(input_pipe) == 0 && pipe(output_pipe) == 0 && pipe(error_pipe) == 0);
    if (!ok) std::cerr << "[CGI] Pipe creation error\n";
    return ok;
}

void CGIHandler::setup_child_process(const std::string& absPath, int input_pipe[2], int output_pipe[2], int error_pipe[2]) {
    // Redirect stdin
    if (dup2(input_pipe[0], STDIN_FILENO) == -1) {
        perror("[CGI] dup2 STDIN failed");
        exit(1);
    }
    close(input_pipe[0]);
    close(input_pipe[1]);

    // Redirect stdout
    if (dup2(output_pipe[1], STDOUT_FILENO) == -1) {
        perror("[CGI] dup2 STDOUT failed");
        exit(1);
    }
    close(output_pipe[0]);
    close(output_pipe[1]);

    // Redirect stderr
    if (dup2(error_pipe[1], STDERR_FILENO) == -1) {
        perror("[CGI] dup2 STDERR failed");
        exit(1);
    }
    close(error_pipe[0]);
    close(error_pipe[1]);

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

    // Debug: print envp
    std::cerr << "[CGI] Executing with environment variables:\n";
    for (size_t i = 0; i < envp.size() - 1; ++i) {
        std::cerr << "[CGI] " << envp[i] << std::endl;
    }
    std::cerr << "[CGI] execve path: " << absPath << std::endl;

    execve(absPath.c_str(), argv, envp.data());
    perror("[CGI] execve failed");
    fprintf(stderr, "[CGI] execve failed for script: %s\n", absPath.c_str());
    exit(127); // Standard for execve failure
}

void CGIHandler::send_input_to_cgi(int input_fd) const {
    if (!inputBody.empty()) {
        ssize_t written = write(input_fd, inputBody.c_str(), inputBody.size());
        std::cerr << "[CGI] Sent " << written << " bytes to CGI stdin\n";
    }
}

std::string CGIHandler::read_from_pipe(int fd) const {
    std::string result;
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        result.append(buffer, bytesRead);
    return result;
}

bool CGIHandler::check_child_status(int status, const std::string& error_output) const {
    if (WIFSIGNALED(status)) {
        std::cerr << "[CGI] CGI script killed by signal: " << WTERMSIG(status) << std::endl;
        if (!error_output.empty())
            std::cerr << "[CGI] CGI script stderr: " << error_output << std::endl;
        return false;
    }
    if (!WIFEXITED(status)) {
        std::cerr << "[CGI] CGI script did not exit normally." << std::endl;
        if (!error_output.empty())
            std::cerr << "[CGI] CGI script stderr: " << error_output << std::endl;
        return false;
    }
    std::cerr << "[CGI] WIFEXITED: " << WIFEXITED(status) << ", WEXITSTATUS: " << WEXITSTATUS(status) << std::endl;
    if (WEXITSTATUS(status) != 0) {
        std::cerr << "[CGI] CGI script exited with status: " << WEXITSTATUS(status) << std::endl;
        if (!error_output.empty())
            std::cerr << "[CGI] CGI script stderr: " << error_output << std::endl;
        return false;
    }
    return true;
}

bool CGIHandler::validate_cgi_headers(const std::string& output) const {
    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos)
        header_end = output.find("\n\n");
    std::cerr << "[CGI DEBUG] header_end: " << header_end << std::endl;
    if (header_end == std::string::npos)
        return false;

    std::string headers = output.substr(0, header_end);
    std::cerr << "[CGI DEBUG] headers substring: [" << headers << "]" << std::endl;
    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        std::cerr << "[CGI DEBUG] header line: [" << line << "]" << std::endl;
        // Remove trailing \r if present
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.find("Content-Type:") == 0 || line.find("content-type:") == 0)
            return true;
    }
    return false;
}
