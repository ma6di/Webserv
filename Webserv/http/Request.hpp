#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

class Request {
public:
    Request(const std::string& raw_data);
    std::string getMethod() const;
    std::string getPath() const;
    std::string getVersion() const;
    std::string getHeader (const std::string& key) const;
    std::string getBody() const;

private:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    void parseRequest(const std::string& raw_data);
};

#endif
