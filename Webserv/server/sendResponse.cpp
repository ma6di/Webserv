#include "WebServer.hpp"

extern Config g_config;

// Send a file as a response (with correct Content-Type)
void WebServer::send_file_response(int client_fd, const std::string& path, size_t i) {
    std::string body = read_file(path);
    if (body.empty()) {
        send_error_response(client_fd, 404, "Not Found", i);
        return;
    }
    Response(client_fd, 200, "OK", body, single_header("Content-Type", get_mime_type(path)));
    cleanup_client(client_fd, i);
}

// Send a redirect response
void WebServer::send_redirect_response(int client_fd, int code, const std::string& location, size_t i) {
    std::ostringstream body;
    body << "<html><head><title>" << code << " Redirect</title></head><body>"
         << "<h1>" << code << " Redirect</h1>"
         << "<p>Redirecting to <a href=\"" << location << "\">" << location << "</a></p>"
         << "</body></html>";
    Response(client_fd, code, "Redirect", body.str(), redirect_headers(location));
    cleanup_client(client_fd, i);
}

void WebServer::send_ok_response(int client_fd, const std::string& body, const std::map<std::string, std::string>& headers, size_t i) {
    Response(client_fd, 200, "OK", body, headers);
    cleanup_client(client_fd, i);
}

void WebServer::send_upload_success_response(int client_fd, const std::string& full_filename, size_t i) {
    std::ostringstream body;
    body << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
         << "<title>Upload Successful</title></head><body style=\"font-family:sans-serif;text-align:center;margin-top:50px;\">"
         << "<h1>✅ File uploaded successfully!</h1>"
         << "<p>Saved as: <code>" << full_filename << "</code></p>"
         << "<br><br>"
         << "<a href=\"/\" style=\"margin: 0 10px;\"><button>Home</button></a>"
         << "<a href=\"/about.html\" style=\"margin: 0 10px;\"><button>About</button></a>"
         << "<a href=\"/static/upload.html\" style=\"margin: 0 10px;\"><button>Upload Another</button></a>"
         << "</body></html>";

    send_ok_response(client_fd, body.str(), single_header("Content-Type", "text/html; charset=utf-8"), i);
}
