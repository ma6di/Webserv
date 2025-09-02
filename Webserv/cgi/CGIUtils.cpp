#include "CGIHandler.hpp"

volatile sig_atomic_t g_cgi_alarm_fired = 0;
void cgi_alarm_handler(int) { g_cgi_alarm_fired = 1; }

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

void CGIHandler::log_cgi_debug(int status, int ret, const std::string& output, const std::string& error_output) const {
    Logger::log(LOG_DEBUG, "CGIHandler", "waitpid returned: " + to_str(ret) + ", status: " + to_str(status));
    Logger::log(LOG_DEBUG, "CGIHandler", "WIFEXITED: " + to_str(WIFEXITED(status)) + ", WEXITSTATUS: " + to_str(WEXITSTATUS(status)));
    Logger::log(LOG_DEBUG, "CGIHandler", "WIFSIGNALED: " + to_str(WIFSIGNALED(status)) + ", WTERMSIG: " + to_str(WTERMSIG(status)));

    Logger::log(LOG_DEBUG, "CGIHandler", "[CGI DEBUG] CGI ERROR output:\n" + error_output + "\n[END]");
    (void)output;
}

std::map<std::string, std::string> CGIHandler::build_cgi_env(const Request& request,
                                                             const std::string& script_name,
                                                             const std::string& path_info) {
    std::map<std::string, std::string> env;
    std::string uri = request.getPath();
    std::string method = request.getMethod();
    size_t q = uri.find('?');
    std::string query_string = (q == std::string::npos) ? "" : uri.substr(q + 1);

    // REQUEST_METHOD: HTTP method (GET, POST, etc.)
    // Used by all CGI scripts to determine request type
    env["REQUEST_METHOD"] = method;

    // SCRIPT_NAME: The URI path to the CGI script
    env["SCRIPT_NAME"] = script_name;

    // QUERY_STRING: The part of the URI after '?'
    // Used for GET requests to pass parameters
    env["QUERY_STRING"] = query_string;

    // PATH_INFO: Extra path info after the script name
    // Used for RESTful APIs or scripts that process sub-paths
    env["PATH_INFO"] = path_info;

    if (method == "POST") {
        // CONTENT_LENGTH: Length of the request body (for POST)
        // Tells the script how much data to read from stdin
        std::ostringstream oss;
        oss << request.getBody().size();
        env["CONTENT_LENGTH"] = oss.str();

        // CONTENT_TYPE: MIME type of the request body (for POST)
        // Tells the script how to interpret the incoming data
        env["CONTENT_TYPE"] = request.getHeader("Content-Type");
    }

    // GATEWAY_INTERFACE: CGI version (always "CGI/1.1")
    // Standard for all CGI scripts
    env["GATEWAY_INTERFACE"] = "CGI/1.1";

    // SERVER_PROTOCOL: HTTP protocol version (e.g., "HTTP/1.1")
    env["SERVER_PROTOCOL"] = "HTTP/1.1";

    // SERVER_SOFTWARE: Identifies the server software
    env["SERVER_SOFTWARE"] = "Webserv/1.0";

    // REDIRECT_STATUS: Used by PHP CGI to indicate a successful redirect (usually "200")
    // Required for PHP CGI, ignored by Python CGI
    env["REDIRECT_STATUS"] = "200";

    return env;
}

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