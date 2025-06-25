#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include <sstream>
#include <unistd.h>
// Type alias for readability
typedef std::map<std::string, std::string> Headers;

class Response {
public:
    Response();
    Response(int client_fd, int code, const std::string& message, const std::string& body,
             const std::map<std::string, std::string>& extra_headers = std::map<std::string, std::string>());

    void setStatus(int code, const std::string& message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    std::string toString() const;

private:
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;
};

// Utility header helpers
std::map<std::string, std::string> single_header(const std::string& k, const std::string& v);
std::map<std::string, std::string> content_type_html();
std::map<std::string, std::string> content_type_json();
std::map<std::string, std::string> redirect_headers(const std::string& url);

#endif
