#ifndef ROUTE_HPP
#define ROUTE_HPP

#include <string>
#include <vector>

class Route {
public:
    Route(const std::string& path);

    const std::string& getPath() const;
    const std::string& getRoot() const;
    const std::string& getIndex() const;
    const std::vector<std::string>& getAllowedMethods() const;
    const std::string& getCGIScript() const;

    void setRoot(const std::string& root);
    void setIndex(const std::string& index);
    void setAllowedMethods(const std::vector<std::string>& methods);
    void setCGIScript(const std::string& cgi);

private:
    std::string path;
    std::string root;
    std::string index;
    std::vector<std::string> allowed_methods;
    std::string cgi_script;

};

#endif