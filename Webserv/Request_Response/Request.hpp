#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <sstream>
#include <iostream>

//This file defines the Request class — it represents a parsed HTTP request from the client.
class Request {
public:
    Request(const std::string& raw_data);
    std::string getMethod() const;
    std::string getPath() const;
    std::string getVersion() const;
    std::string getHeader (const std::string& key) const;
    std::string getBody() const;
    void setBody(const std::string& newBody);
	bool isChunked() const;
    bool isValidHttpVersionFormat(const std::string& version) const;
    bool hasExpectContinue() const;

    int getContentLength() const; // <-- Add this

private:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    int content_length; // <-- Add this

    void parseRequest(const std::string& raw_data);
};

#endif
