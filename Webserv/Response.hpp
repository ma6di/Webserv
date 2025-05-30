#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>

class Response {
public:
    Response();
    void setStatus(int code, const std::string& message);
    void setBody(const std::string& body);
    void setHeader(const std::string& key, const std::string& value);
    std::string build() const;

private:
    int status_code;
    std::string status_message;
    std::string body;
    std::map<std::string, std::string> headers;
}; 

#endif