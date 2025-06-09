#include "Route.hpp"

Route::Route(const std::string& path) : path(path), index ("index.html"), allowed_methods(1, "GET") {}

const std::string& Route::getPath() const {
    return path;
}

const std::string& Route::getRoot() const {
    return root;
}

const std::string& Route::getIndex() const {
    return index;
}

const std::vector<std::string>& Route::getAllowedMethods() const {
    return allowed_methods;
}

const std::string& Route::getCGIScript() const {
    return cgi_script;
}

void Route::setRoot(const std::string& root) {
    this->root = root;
}

void Route::setIndex(const std::string& index) {
    this->index = index;
}

void Route::setAllowedMethods(const std::vector<std::string>& methods) {
    this->allowed_methods = methods;
}

void Route::setCGIScript(const std::string& cgi) {
    this->cgi_script = cgi;
}