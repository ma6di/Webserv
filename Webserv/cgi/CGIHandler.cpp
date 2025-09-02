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


CGIHandler::CGIHandler(const std::string& scriptPath,
                       const std::map<std::string, std::string>& env,
                       const std::string& inputBody,
                       const std::string& requestedUri)
    : scriptPath(scriptPath), environment(env), inputBody(inputBody), requestedUri(requestedUri) {}

std::string CGIHandler::execute() {
    std::string absPath = resolve_script_path();
    int input_pipe[2], output_pipe[2], error_pipe[2];
    if (!create_pipes(input_pipe, output_pipe, error_pipe)) {
        Logger::log(LOG_ERROR, "CGIHandler", "Pipe creation failed");
        throw std::runtime_error("Pipe creation failed");
    }

    pid_t pid = fork();
    if (pid < 0) {
        Logger::log(LOG_ERROR, "CGIHandler", "Fork failed");
        throw std::runtime_error("Fork failed");
    }

    if (pid == 0) {
        setup_child_process(absPath, input_pipe, output_pipe, error_pipe);
    }

    close(input_pipe[0]);
    close(output_pipe[1]);
    close(error_pipe[1]);

    send_input_to_cgi(input_pipe[1]);
    close(input_pipe[1]);

    int status = 0;
    bool timed_out = false;
    int ret = wait_for_child_with_timeout(pid, status, timed_out);
    (void)ret; // suppress unused warning
    if (timed_out) {
        handle_timeout(pid, status);
        return "__CGI_TIMEOUT__";
    }

    std::string output = read_pipe_to_string(output_pipe[0]);
    close(output_pipe[0]);
    std::string error_output = read_from_pipe(error_pipe[0]);
    close(error_pipe[0]);

    //log_cgi_debug(status, ret, output, error_output);

    if (!check_child_status(status, error_output))
        return "__CGI_INTERNAL_ERROR__";

    if (!validate_cgi_headers(output))
        return "__CGI_MISSING_HEADER__";

    return output;
}

/*std::string CGIHandler::read_pipe_to_string(int fd) const {
    std::string result;
    char buffer[4096];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        result.append(buffer, n);
    }
    return result;
}*/

std::string CGIHandler::read_pipe_to_string(int fd) const {
    std::string result;
    char buffer[4096];

    while (true) {
        ssize_t n = ::read(fd, buffer, sizeof(buffer));

        if (n > 0) {
            // Got data → append it
            result.append(buffer, static_cast<size_t>(n));
        }
        else if (n == 0) {
            // EOF → pipe closed
            break;
        }
        else if (n < 0) {
            // Error case → just stop (do not check errno)
            break;
        }
    }

    return result;
}

// --- Static helpers for CGI logic ---
/*It checks if the request URI starts with the CGI location URI (cgi_uri). If not, it returns false.
It extracts the part of the URI after the CGI location (rel_uri).
It loops through possible script candidates in rel_uri, trying to find an executable file in cgi_root.
For each candidate, it checks if the file exists and is executable using access(abs_candidate.c_str(), X_OK).
If it finds a match, it sets script_path, script_name, and path_info (the extra path after the script).
If no executable file is found, it returns false.*/
bool CGIHandler::find_cgi_script(const std::string& cgi_root, const std::string& cgi_uri, const std::string& uri,
                                 std::string& script_path, std::string& script_name, std::string& path_info) {
    if (uri.find(cgi_uri) != 0)
        return false;
    std::string rel_uri = uri.substr(cgi_uri.length());
    size_t match_len = 0;
    for (size_t pos = rel_uri.size(); pos > 0; --pos) {
        if (rel_uri[pos - 1] == '/')
            continue;
        std::string candidate = rel_uri.substr(0, pos);
        std::string abs_candidate = cgi_root + candidate;
        if (access(abs_candidate.c_str(), X_OK) == 0) {
            script_path = abs_candidate;
            script_name = cgi_uri + candidate;
            match_len = pos;
            break;
        }
    }
    if (script_path.empty())
        return false;
    path_info = rel_uri.substr(match_len);
    return true;
}

void CGIHandler::parse_cgi_output(const std::string& cgi_output, std::map<std::string, std::string>& cgi_headers, std::string& body) {
    size_t header_end = cgi_output.find("\r\n\r\n");
    size_t sep_len = 4;
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        sep_len = 2;
    }
    if (header_end == std::string::npos) {
        cgi_headers.clear();
        body.clear();
        return;
    }
    std::string headers = cgi_output.substr(0, header_end);
    body = cgi_output.substr(header_end + sep_len);

    std::istringstream header_stream(headers);
    std::string line;
    bool has_content_type = false;
    while (std::getline(header_stream, line)) {
        if (line.empty() || line == "\r")
            continue;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
                value.erase(0, 1);
            std::string key_lower = key;
            for (size_t j = 0; j < key_lower.length(); ++j)
                key_lower[j] = (char)std::tolower((unsigned char)key_lower[j]);
            if (key_lower == "content-type")
                has_content_type = true;
            cgi_headers[key] = value;
        }
    }
    if (!has_content_type) {
        cgi_headers["Content-Type"] = "text/html";
    }
}

bool CGIHandler::create_pipes(int input_pipe[2], int output_pipe[2], int error_pipe[2]) const {
    bool ok = (pipe(input_pipe) == 0 && pipe(output_pipe) == 0 && pipe(error_pipe) == 0);
    if (!ok) Logger::log(LOG_ERROR, "CGIHandler", "Pipe creation error");
    return ok;
}

void CGIHandler::setup_child_process(const std::string& absPath, int input_pipe[2], int output_pipe[2], int error_pipe[2]) {
    if (dup2(input_pipe[0], STDIN_FILENO) == -1) {
        perror("[CGI] dup2 STDIN failed");
        exit(1);
    }
    close(input_pipe[0]);
    close(input_pipe[1]);

    if (dup2(output_pipe[1], STDOUT_FILENO) == -1) {
        perror("[CGI] dup2 STDOUT failed");
        exit(1);
    }
    close(output_pipe[0]);
    close(output_pipe[1]);

    if (dup2(error_pipe[1], STDERR_FILENO) == -1) {
        perror("[CGI] dup2 STDERR failed");
        exit(1);
    }
    close(error_pipe[0]);
    close(error_pipe[1]);

    size_t last_slash = absPath.find_last_of('/');
    if (last_slash != std::string::npos) {
        std::string script_dir = absPath.substr(0, last_slash);
        if (chdir(script_dir.c_str()) != 0) {
            perror("[CGI] chdir to script directory failed");
            exit(1);
        }
    }

    std::string interpreter;
    if (absPath.size() >= 4 && absPath.substr(absPath.size() - 4) == ".php") {
        environment["SCRIPT_FILENAME"] = absPath;
        interpreter = "/usr/bin/php-cgi"; // Adjust path if needed
    }

    std::vector<std::string> envStrings;
    for (std::map<std::string, std::string>::const_iterator it = environment.begin(); it != environment.end(); ++it)
        envStrings.push_back(it->first + "=" + it->second);

    std::vector<char*> envp;
    for (size_t i = 0; i < envStrings.size(); ++i)
        envp.push_back(const_cast<char*>(envStrings[i].c_str()));
    envp.push_back(NULL);

    char* argv[4];
    if (!interpreter.empty()) {
        argv[0] = const_cast<char*>(interpreter.c_str());
        argv[1] = NULL;
    } else {
        argv[0] = const_cast<char*>(absPath.c_str());
        argv[1] = const_cast<char*>(requestedUri.c_str());
        argv[2] = NULL;
    }

    if (!interpreter.empty())
        execve(interpreter.c_str(), argv, &envp[0]);
    else
        execve(absPath.c_str(), argv, &envp[0]);

    perror("[CGI] execve failed");
    fprintf(stderr, "[CGI] execve failed for script: %s\n", absPath.c_str());
    exit(127);
}

void CGIHandler::send_input_to_cgi(int input_fd) const {
        if (inputBody.empty())
        return;

    ssize_t n = ::write(input_fd, inputBody.c_str(), inputBody.size());

    if (n > 0) {
        // Successfully wrote some or all data
        // (we don’t loop for partial writes here, since simplicity is fine)
        Logger::log(LOG_INFO, "CGIHandler",
                    "Wrote " + to_str(n) + " bytes to CGI stdin");
    }
    else if (n == 0) {
        // Pipe closed by peer
        Logger::log(LOG_INFO, "CGIHandler",
                    "Pipe closed while writing to CGI stdin");
    }
    else if (n < 0) {
        // Error case (don’t check errno)
        Logger::log(LOG_ERROR, "CGIHandler",
                    "Write to CGI stdin failed");
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
        Logger::log(LOG_ERROR, "CGIHandler", "CGI script killed by signal: " + to_str(WTERMSIG(status)));
        if (!error_output.empty())
            Logger::log(LOG_ERROR, "CGIHandler", "CGI script stderr: " + error_output);
        return false;
    }
    if (!WIFEXITED(status)) {
        Logger::log(LOG_ERROR, "CGIHandler", "CGI script did not exit normally.");
        if (!error_output.empty())
            Logger::log(LOG_ERROR, "CGIHandler", "CGI script stderr: " + error_output);
        return false;
    }
    //Logger::log(LOG_DEBUG, "CGIHandler", "WIFEXITED: " + to_str(WIFEXITED(status)) + ", WEXITSTATUS: " + to_str(WEXITSTATUS(status)));
    if (WEXITSTATUS(status) != 0) {
        Logger::log(LOG_ERROR, "CGIHandler", "CGI script exited with status: " + to_str(WEXITSTATUS(status)));
        if (!error_output.empty())
            Logger::log(LOG_ERROR, "CGIHandler", "CGI script stderr: " + error_output);
        return false;
    }
    return true;
}

bool CGIHandler::validate_cgi_headers(const std::string& output) const {
    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos)
        header_end = output.find("\n\n");
    if (header_end == std::string::npos)
        return false;

    std::string headers = output.substr(0, header_end);
    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        std::string lower_line = line;
        std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
        if (lower_line.find("content-type:") == 0)
            return true;
    }
    return false;
}
