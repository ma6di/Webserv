#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Router.hpp"
#include "utils.hpp"

WebServer::WebServer(int port) : port(port) {
    setup_server_socket(port);
    setup_routes();
}

WebServer::~WebServer() {
    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
}

void    WebServer::run() {
    std::cout << "Server running on http://localhost:" << port << "\n";
    poll_loop();
}

void WebServer::setup_server_socket(int port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        throw std::runtime_error("Socker creation failed");
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
        throw std::runtime_error("Bind failed");
    if (listen(server_fd, 10) < 0)
        throw std::runtime_error("Listen failed");

    make_socket_non_blocking(server_fd);   
}

void    WebServer::make_socket_non_blocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set O_NONBLOCK");
}

void    WebServer::poll_loop() {
    fds.clear();
    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    while(1) {
        int count = poll(fds.data(), fds.size(), -1);
        if (count < 0) {
            std::cerr << "Poll failed\n";
            break;
        }
        for (size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;
            if (fds[i].fd == server_fd)
                handle_new_connection();
            else
                handle_client_data(i--);
        }
    }
}

void    WebServer::handle_new_connection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Accept failed\n";
        return;
    }
    make_socket_non_blocking(client_fd);
    
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);
        
    std::cout << "New client connected: FD=" << client_fd << "\n"; 
}

void    WebServer::handle_client_data(size_t i) {
    int client_fd = fds[i].fd;
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    int bytes = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes <= 0) {
        std::cout << "Client disconnected: FD= " << client_fd << "\n";
        close(client_fd);
        fds.erase(fds.begin() + i);
        return;
    } 
    
    try {
        std::string raw_request(buffer);
        Request req(raw_request);
        req.debugPrint();
        //std::string path = req.getPath();
        send_response(client_fd, req);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse HTTP request: " << e.what() << "\n"; 
    }
        fds.erase(fds.begin() + i);
}    

/*void WebServer::send_response(int client_fd, const std::string& raw_path) {
    Response res;
    const Route* route = router.matchRoute(raw_path);
    std::string file_path;

    if (route) {
        std::string path = raw_path;
        if (path.length() > 1 && path[path.length() - 1] == '/')
            path = path.substr(0, path.length() - 1);

        if (path == route->getPath()) {
            file_path = route->getRoot() + "/" + route->getIndex();
        } else {
            file_path = route->getRoot() + raw_path;
        }
    } else {
        file_path = resolve_path(raw_path);
    }

    std::cout << "Resolved file_path: " << file_path << "\n";

    struct stat path_stat;
    std::string body;

    if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        body = generate_directory_listing(file_path, raw_path);
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");    
    } else {
        std::ifstream file(file_path.c_str());
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();

            res.setStatus(200, "OK");
            res.setHeader("Content-Type", get_mime_type(file_path));
        } else {
            body = "<h1>404 Not Found</h1>";
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/html");
        }
    }

    std::ostringstream length_ss;
    length_ss << body.size();
    res.setHeader("Content-Length", length_ss.str());
    res.setHeader("Connection", "close");
    res.setBody(body);

    std::string full_response = res.build();
    std::cout << "=== Full Response ===\n" << full_response << "\n=== END ===\n";

    write(client_fd, full_response.c_str(), full_response.size());
}*/

/*void WebServer::send_response(int client_fd, const std::string& raw_data) {
    Request req(raw_data);
    Response res;

    const std::string& method = req.getMethod();
    const std::string& raw_path = req.getPath();

    const Route* route = router.matchRoute(raw_path);
    if (route && !is_method_allowed(method, route->getAllowedMethods())) {
        res.setStatus(405, "Method Not Allowed");
        res.setBody("<h1>405 Method Not Allowed</h1>");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Allow", join_methods(route->getAllowedMethods()));
        finalize_response_and_send(client_fd, res);
        return;
    }

    std::string file_path = resolve_final_path(raw_path, route);
    std::cout << "Resolved file_path: " << file_path << "\n";

    std::string body;
    struct stat path_stat;

    if (file_path == "./nonexistent") {
        res.setStatus(404, "Not Found");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>404 Not Found</h1>");
        finalize_response_and_send(client_fd, res);
        return;
    }

    if (file_path.empty()) {
        res.setStatus(404, "Not Found");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>404 Not Found</h1>");
        finalize_response_and_send(client_fd, res);
        return;
    }

    if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        body = generate_directory_listing(file_path, raw_path);
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
    } else {
        std::ifstream file(file_path.c_str());
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();
            res.setStatus(200, "OK");
            res.setHeader("Content-Type", get_mime_type(file_path));
        } else {
            body = "<h1>404 Not Found</h1>";
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/html");
        }
    }

    res.setBody(body);
    finalize_response_and_send(client_fd, res);
}*/

/*void WebServer::send_response(int client_fd, const std::string& raw_data) {
    Request req(raw_data);
    Response res;

    const std::string& method = req.getMethod();
    const std::string& raw_path = req.getPath();
    std::cout << "Method: " << req.getMethod() << ", Path: " << req.getPath() << "\n";

    const Route* route = router.matchRoute(raw_path);
    if (route && !is_method_allowed(method, route->getAllowedMethods())) {
        res.setStatus(405, "Method Not Allowed");
        res.setBody("<h1>405 Method Not Allowed</h1>");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Allow", join_methods(route->getAllowedMethods()));
        finalize_response_and_send(client_fd, res);
        return;
    }

    std::string file_path = resolve_final_path(raw_path, route);
    std::cout << "Resolved file_path: " << file_path << "\n";

    struct stat path_stat;
    std::string body;

    if (stat(file_path.c_str(), &path_stat) == 0) {
        if (S_ISREG(path_stat.st_mode)) {
            std::ifstream file(file_path.c_str());
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();
            res.setStatus(200, "OK");
            res.setHeader("Content-Type", get_mime_type(file_path));
        } else if (S_ISDIR(path_stat.st_mode)) {
            std::string index_path = file_path + "/index.html";
            if (stat(index_path.c_str(), &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
                std::ifstream file(index_path.c_str());
                std::ostringstream buffer;
                buffer << file.rdbuf();
                body = buffer.str();
                res.setStatus(200, "OK");
                res.setHeader("Content-Type", "text/html");
            } else {
                body = generate_directory_listing(file_path, raw_path);
                res.setStatus(200, "OK");
                res.setHeader("Content-Type", "text/html");
            }
        } else {
            res.setStatus(403, "Forbidden");
            body = "<h1>403 Forbidden</h1>";
            res.setHeader("Content-Type", "text/html");
        }
    } else {
        res.setStatus(404, "Not Found");
        body = "<h1>404 Not Found</h1>";
        res.setHeader("Content-Type", "text/html");
    }

    res.setBody(body);
    finalize_response_and_send(client_fd, res);
}*/

/*void WebServer::send_response(int client_fd, const std::string& raw_data) {
    Request req(raw_data);
    Response res;

    const std::string& method = req.getMethod();
    const std::string& raw_path = req.getPath();
    std::cout << "[DEBUG] Raw path extracted: '" << raw_path << "'\n";
    if (req.getMethod().empty() || req.getPath().empty()) {
    std::cerr << "[ERROR] Invalid request. Skipping.\n";
    return;
}

    const Route* route = router.matchRoute(raw_path);
    if (route && !is_method_allowed(method, route->getAllowedMethods())) {
        res.setStatus(405, "Method Not Allowed");
        res.setBody("<h1>405 Method Not Allowed</h1>");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Allow", join_methods(route->getAllowedMethods()));
        finalize_response_and_send(client_fd, res);
        return;
    }

    std::string file_path = resolve_final_path(raw_path, route);
    std::cout << "Resolved file_path: " << file_path << "\n";

    struct stat path_stat;
    std::string body;

    if (file_path == "./nonexistent" || file_path.empty()) {
        res.setStatus(404, "Not Found");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>404 Not Found</h1>");
    } else if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        body = generate_directory_listing(file_path, raw_path);
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody(body);
    } else {
        std::ifstream file(file_path.c_str());
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();
            res.setStatus(200, "OK");
            res.setHeader("Content-Type", get_mime_type(file_path));
            res.setBody(body);
        } else {
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/html");
            res.setBody("<h1>404 Not Found</h1>");
        }
    }

    finalize_response_and_send(client_fd, res);
}*/

void WebServer::send_response(int client_fd, const Request& req) {
    Response res;

    const std::string& method = req.getMethod();
    const std::string& raw_path = req.getPath();

    const Route* route = router.matchRoute(raw_path);
    if (route && !is_method_allowed(method, route->getAllowedMethods())) {
        res.setStatus(405, "Method Not Allowed");
        res.setBody("<h1>405 Method Not Allowed</h1>");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Allow", join_methods(route->getAllowedMethods()));
        finalize_response_and_send(client_fd, res);
        return;
    }

    std::string file_path = resolve_final_path(raw_path, route);
    std::cout << "Resolved file_path: " << file_path << "\n";

    struct stat path_stat;
    std::string body;

    if (file_path.empty() || file_path == "./nonexistent") {
        res.setStatus(404, "Not Found");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>404 Not Found</h1>");
        finalize_response_and_send(client_fd, res);
        return;
    }

    if (stat(file_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        body = generate_directory_listing(file_path, raw_path);
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
    } else {
        std::ifstream file(file_path.c_str());
        if (file) {
            std::ostringstream buffer;
            buffer << file.rdbuf();
            body = buffer.str();
            res.setStatus(200, "OK");
            res.setHeader("Content-Type", get_mime_type(file_path));
        } else {
            res.setStatus(404, "Not Found");
            res.setHeader("Content-Type", "text/html");
            res.setBody("<h1>404 Not Found</h1>");
            finalize_response_and_send(client_fd, res);
            return;
        }
    }

    res.setBody(body);
    finalize_response_and_send(client_fd, res);
}



void    WebServer::setup_routes() {
    Route root_route("/");
    root_route.setRoot("./www");
    root_route.setIndex("index.html");
    std::vector<std::string> methods;
    methods.push_back("GET");
    methods.push_back("HEAD");
    root_route.setAllowedMethods(methods);
    router.addRoute(root_route);

    Route contact_route("/contact");
    contact_route.setRoot("./www");
    contact_route.setIndex("contact.html");
    std::vector<std::string> contact_methods;
    contact_methods.push_back("GET");
    contact_methods.push_back("HEAD");
    contact_route.setAllowedMethods(contact_methods);
    router.addRoute(contact_route);
}

std::string WebServer::resolve_final_path(const std::string& raw_path, const Route* route) {
    if (!route)
        return "./nonexistent";

    std::string clean_path = raw_path;
    if (clean_path.length() > 1 && clean_path[clean_path.size() - 1] == '/')
        clean_path = clean_path.substr(0, clean_path.size() - 1);

    if (clean_path == route->getPath())
        return route->getRoot() + "/" + route->getIndex();

    std::string relative_path = clean_path.substr(route->getPath().length());
    if (!relative_path.empty() && relative_path[0] == '/')
        relative_path = relative_path.substr(1);

    return route->getRoot() + "/" + relative_path;
}




void WebServer::finalize_response_and_send(int client_fd, Response& res) {
    std::ostringstream length_ss;
    length_ss << res.getBody().size();
    res.setHeader("Content-Length", length_ss.str());
    res.setHeader("Connection", "close");

    std::string full_response = res.build();
    std::cout << "=== Full Response ===\n" << full_response << "\n=== END ===\n";
    write(client_fd, full_response.c_str(), full_response.size());
}

std::string WebServer::join_methods(const std::vector<std::string>& methods) const {
    std::string result;
    for (size_t i = 0; i < methods.size(); ++i) {
        result += methods[i];
        if (i != methods.size() - 1)
            result += ", ";
    }
    return result;
}
