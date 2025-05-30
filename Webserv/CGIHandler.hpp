#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>

class CGIHandler {
public:
    CGIHandler(const std::string& scriptPath, const std::map<std::string, std::string>& enf);
    std::string execute();

private:
    std::string scriptPath;
    std::map<std::string, std::string> environment;

    std::string runCGI(); 
};

#endif

//just a basic starter. needs to be updated. 