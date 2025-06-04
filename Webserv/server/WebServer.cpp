#include "WebServer.hpp"
#include <string>


extern Config g_config;

//Initializes port, and calls the method to create and configure the server socket.
WebServer::WebServer(int port) : port(port) {
    setup_server_socket(port);
}

//Cleans up all open sockets when the server object is destroyed.
WebServer::~WebServer() {
    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
}

//Start the Event Loop
void    WebServer::run() {
    std::cout << "Server running on http://localhost:" << port << "\n";
    poll_loop();
}

//Setup Listening Socket
//SOCK_STREAM means TCP
	//bind() to INADDR_ANY makes it listen on all interfaces
	//listen() enables incoming connections
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
	//Ensures non-blocking I/O for poll-based async logic
    make_socket_non_blocking(server_fd);   
    //return (server_fd);
}

void    WebServer::make_socket_non_blocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set O_NONBLOCK");
}

//Monitors multiple sockets (both server socket and clients)
//If something becomes "readable", handle it
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
		//Check if the event was a read (data available)
        for (size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN))
                continue;
			//If it's the server socket, accept new client
			//Otherwise, process incoming client data
            if (fds[i].fd == server_fd)
                handle_new_connection();
            else
                handle_client_data(i--);
        }
    }
}

//Accepts a new client connection
void    WebServer::handle_new_connection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Accept failed\n";
        return;
    }
	//Also sets client to non-blocking
    make_socket_non_blocking(client_fd);
    //Add to the list of monitored sockets
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);
        
    std::cout << "New client connected: FD=" << client_fd << "\n"; 
}

void WebServer::handle_client_data(size_t i) {
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

	//Use your Request class to parse the method, path, headers, etc.
    std::string request_data(buffer);
    try {
        Request request(request_data);  // Parse the HTTP request line and headers
        std::string uri = request.getPath();

		//Find the matching location {} block for this URI.
        const LocationConfig* loc = match_location(g_config.getLocations(), uri);

		std::string method = request.getMethod();
		const std::vector<std::string>& allowed = loc->allowed_methods;

		bool methodAllowed = false;
		for (size_t j = 0; j < allowed.size(); ++j) {
			if (allowed[j] == method) {
				methodAllowed = true;
				break;
			}
		}
		//		//If the HTTP method is not allowed for this location → return 405.
		if (!methodAllowed) {
			Response res;
			res.setStatus(405, "Method Not Allowed");
			res.setBody("<h1>405 Method Not Allowed</h1>");
			std::string raw = res.toString();
			write(client_fd, raw.c_str(), raw.size());
			close(client_fd);
			fds.erase(fds.begin() + i);
			return;
		}
		//If it’s not CGI, serve as a static file.
        if (loc && is_cgi_request(*loc, uri)) {
            std::string script_path = resolve_script_path(uri, *loc);

            std::map<std::string, std::string> env;
            env["REQUEST_METHOD"] = request.getMethod();
            env["SCRIPT_NAME"] = uri;
            env["QUERY_STRING"] = "";  // Optionally parse it
            std::ostringstream oss;
			oss << request.getBody().size();
			env["CONTENT_LENGTH"] = oss.str();

            CGIHandler handler(script_path, env, request.getBody());
            std::string cgi_output = handler.execute();

            write(client_fd, cgi_output.c_str(), cgi_output.size());
        } else {
            // Serve static file (not CGI)
            send_response(client_fd, uri);  // Your existing function
        }

    } catch (const std::exception& e) {
        std::cerr << "Request parse error: " << e.what() << "\n";
        write(client_fd, "HTTP/1.1 400 Bad Request\r\n\r\n", 28);
    }

    close(client_fd);
    fds.erase(fds.begin() + i);
}

void    WebServer::send_response(int client_fd, const std::string& raw_path) {
    //Convert /about or / to ./www/about.html, etc.
	std::string file_path = resolve_path(raw_path);

    std::ifstream file(file_path.c_str());
    std::string body;
    int status_code;
    std::string status_text;
	//Send 200 or 404 response depending on file existence
    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        body = buffer.str();
        status_code = 200;
        status_text = "OK";
    } else {
        body = "<h1>404 Not Found</h1>";
        status_code = 404;
        status_text = "Not Found";
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << get_mime_type(file_path) << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n" 
        << body;

    std::string response = oss.str();
    write(client_fd, response.c_str(), response.size());
}

//Converts requested path into full path like ./www/about.html
//Also handles fallback like /blog → /blog.html
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
