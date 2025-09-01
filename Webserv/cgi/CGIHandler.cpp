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


volatile sig_atomic_t g_cgi_alarm_fired = 0;
void cgi_alarm_handler(int) { g_cgi_alarm_fired = 1; }

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

    if (timed_out) {
        handle_timeout(pid, status);
        return "__CGI_TIMEOUT__";
    }

    std::string output = read_pipe_to_string(output_pipe[0]);
    close(output_pipe[0]);
    std::string error_output = read_from_pipe(error_pipe[0]);
    close(error_pipe[0]);

    log_cgi_debug(status, ret, output, error_output);

    if (!check_child_status(status, error_output))
        return "__CGI_INTERNAL_ERROR__";

    if (!validate_cgi_headers(output))
        return "__CGI_MISSING_HEADER__";

    return output;
}

// --- Private helpers ---

int CGIHandler::wait_for_child_with_timeout(pid_t pid, int& status, bool& timed_out) {
    int ret = 0;
    int timeout_seconds = 5;
    struct sigaction sa;
    sa.sa_handler = cgi_alarm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    g_cgi_alarm_fired = 0;
    alarm(timeout_seconds);

    timed_out = false;
    while (true) {
        ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) break;
        if (ret == -1) break;
        if (g_cgi_alarm_fired) {
            timed_out = true;
            break;
        }
        usleep(10000); // Sleep 10ms
    }
    alarm(0); // Cancel alarm
    return ret;
}

void CGIHandler::handle_timeout(pid_t pid, int& status) {
    Logger::log(LOG_ERROR, "CGIHandler", "CGI script timed out, killing PID " + to_str(pid));
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0); // Reap
}

std::string CGIHandler::read_pipe_to_string(int fd) const {
    std::string result;
    char buffer[4096];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        result.append(buffer, n);
    }
    return result;
}

void CGIHandler::log_cgi_debug(int status, int ret, const std::string& output, const std::string& error_output) const {
    Logger::log(LOG_DEBUG, "CGIHandler", "waitpid returned: " + to_str(ret) + ", status: " + to_str(status));
    Logger::log(LOG_DEBUG, "CGIHandler", "WIFEXITED: " + to_str(WIFEXITED(status)) + ", WEXITSTATUS: " + to_str(WEXITSTATUS(status)));
    Logger::log(LOG_DEBUG, "CGIHandler", "WIFSIGNALED: " + to_str(WIFSIGNALED(status)) + ", WTERMSIG: " + to_str(WTERMSIG(status)));

    Logger::log(LOG_DEBUG, "CGIHandler", "[CGI DEBUG] CGI ERROR output:\n" + error_output + "\n[END]");
    (void)output;
}

// --- Static helpers for CGI logic ---

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

std::map<std::string, std::string> CGIHandler::build_cgi_env(const Request& request,
                                                             const std::string& script_name,
                                                             const std::string& path_info) {
    std::map<std::string, std::string> env;
    std::string uri = request.getPath();
    std::string method = request.getMethod();
    size_t q = uri.find('?');
    std::string query_string = (q == std::string::npos) ? "" : uri.substr(q + 1);

    env["REQUEST_METHOD"] = method;
    env["SCRIPT_NAME"] = script_name;
    env["QUERY_STRING"] = query_string;
    env["PATH_INFO"] = path_info;
    if (method == "POST") {
        std::ostringstream oss;
        oss << request.getBody().size();
        env["CONTENT_LENGTH"] = oss.str();
        env["CONTENT_TYPE"] = request.getHeader("Content-Type");
    }
    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"] = "HTTP/1.1";
    env["SERVER_SOFTWARE"] = "Webserv/1.0";
    env["REDIRECT_STATUS"] = "200";
    return env;
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

// --- Member helpers for process management ---

std::string CGIHandler::resolve_script_path() const {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        Logger::log(LOG_ERROR, "CGIHandler", "getcwd failed");
        throw std::runtime_error("getcwd failed");
    }
    std::string abs = std::string(cwd) + "/" + scriptPath;
    Logger::log(LOG_DEBUG, "CGIHandler", "Resolved script path: " + abs);
    return abs;
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
    if (!inputBody.empty()) {
        ssize_t written = write(input_fd, inputBody.c_str(), inputBody.size());
        Logger::log(LOG_DEBUG, "CGIHandler", "Sent " + to_str(written) + " bytes to CGI stdin");
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
    Logger::log(LOG_DEBUG, "CGIHandler", "WIFEXITED: " + to_str(WIFEXITED(status)) + ", WEXITSTATUS: " + to_str(WEXITSTATUS(status)));
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
