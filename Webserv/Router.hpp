#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>
#include <vector>
#include <iostream>
#include "Route.hpp"

class Router {
public:
    Router();

    void addRoute(const Route& route);
    const Route* matchRoute(const std::string& path) const;
    std::string resolvePath(const std::string& path) const;

private:
    std::vector<Route> routes;
};

#endif
