#include "Router.hpp"

Router::Router() {}

void Router::addRoute(const Route& route) {
    routes.push_back(route);
}

const Route* Router::matchRoute(const std::string& path) const {
    for (size_t i = 0; i < routes.size(); ++i) {
        const std::string& route_path = routes[i].getPath();

        if (path == route_path || path == route_path + "/")
            return &routes[i];
    }
    return NULL;
}


std::string Router::resolvePath(const std::string& path) const {
    const Route* matched = matchRoute(path);
    if(!matched)
        return "./www" + path;
    
    std::string root = matched->getRoot();
    std::string index = matched->getIndex();
    std::string full_path = root + path.substr(matched->getPath().size());

    if(!full_path.empty() && full_path[full_path.size() - 1] == '/')
        full_path += index;
        
    return full_path;
}
