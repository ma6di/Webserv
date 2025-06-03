#include "WebServer.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "utils.hpp"

WebServer::WebServer(int port) : port(port) {
    setup_server_socket(port);
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
        std::string path = req.getPath();
        send_response(client_fd, path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse HTTP request: " << e.what() << "\n"; 
    }
        fds.erase(fds.begin() + i);
}    

void    WebServer::send_response(int client_fd, const std::string& raw_path) {
    std::string file_path = resolve_path(raw_path);
    std::ifstream file(file_path.c_str());

    Response res;


    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        res.setStatus(200, "OK");
        res.setBody(buffer.str());
        res.setHeader("Content-Type", get_mime_type(file_path));
    } else {
        res.setStatus(404, "Not Found");
        res.setBody("<h1>404 Not Found </h1>");
        res.setHeader("Content-Type", "text/html");
    }

    std::ostringstream content_length_ss;
    content_length_ss << res.build().size();
    res.setHeader("Connection", "close");
    file.close();
    
    std::string full_response = res.build();
    write(client_fd, full_response.c_str(), full_response.size());
}

std::string WebServer::resolve_path(const std::string& raw_path) {
    std::string path = raw_path;

    if (path =="/")
        path = "/index.html";
    else if (!path.empty() && path[path.size() - 1] == '/')
        path += "index.html";
    
    std::string file_path = "./www" + path;
    if (file_exists(file_path))
        return (file_path);
    
    if (path.find('.') == std::string::npos) {
        std::string html_fallback = "./www" + path + ".html";
        if (file_exists(html_fallback))
            return (html_fallback);
    }
    return (file_path);
}
