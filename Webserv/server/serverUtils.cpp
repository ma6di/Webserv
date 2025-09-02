#include "WebServer.hpp"

std::string WebServer::resolve_path(const std::string &raw_path,
                                    const std::string &method,
                                    const LocationConfig *loc)
{
    Logger::log(LOG_DEBUG, "resolve_path",
                "raw_path = \"" + raw_path + "\", method = " + method);

    // 1) Pick the base filesystem root:
    //    - If the location block set a root, use it.
    //    - Otherwise fall back to the server-level root from config_.
    const std::string &serverRoot = config_->getRoot();
    std::string base = (loc && !loc->root.empty())
                           ? loc->root
                           : serverRoot;

    // 2) Strip off the location prefix from the URI, if any
    std::string rel = raw_path;
    if (loc && loc->path != "/" && rel.find(loc->path) == 0)
    {
        rel = rel.substr(loc->path.length());
    }
    if (!rel.empty() && rel[0] == '/')
        rel = rel.substr(1);

    // 3) Build the candidate filesystem path
    std::string candidate = base;
    if (!rel.empty())
        candidate += "/" + rel;

    Logger::log(LOG_DEBUG, "resolve_path", "Candidate path: " + candidate);

    // 4) If it’s a directory, hand it off to handle_directory_request()
    if (is_directory(candidate))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "Directory detected, returning: " + candidate);
        return candidate;
    }

    // 5) If it’s an existing file, return it
    if (file_exists(candidate))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "File exists, returning: " + candidate);
        return candidate;
    }

    // 6) Try adding “.html”
    std::string html_fallback = candidate + ".html";
    if (file_exists(html_fallback))
    {
        Logger::log(LOG_DEBUG, "resolve_path",
                    "HTML fallback, returning: " + html_fallback);
        return html_fallback;
    }

    // 7) Nothing matched: return candidate so your 404 logic fires
    Logger::log(LOG_DEBUG, "resolve_path",
                "Nothing found, returning: " + candidate);
    return candidate;
}

std::string extract_filename(const std::string &header)
{
    size_t pos = header.find("filename=");
    if (pos == std::string::npos)
        return "upload";

    size_t start = header.find('"', pos);
    size_t end = header.find('"', start + 1);
    if (start == std::string::npos || end == std::string::npos)
        return "upload";

    return header.substr(start + 1, end - start - 1);
}

// Reads the entire contents of a file into a string
std::string WebServer::read_file(const std::string &path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file)
    {
        Logger::log(LOG_ERROR, "read_file", "Failed to open file: " + path);
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WebServer::read_and_append_client_data(int client_fd, size_t i)
{
    char buffer[8192];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0)
    {
        cleanup_client(client_fd, i); // EOF
        return false;
    }
    if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;             // not fatal
        cleanup_client(client_fd, i); // real error
        return false;
    }
    conns_[client_fd].readBuf.append(buffer, static_cast<size_t>(bytes_read));

    if (conns_[client_fd].readBuf.size() > config_->getMaxBodySize())
    {
        Logger::log(LOG_ERROR, "read_and_append_client_data", "Payload Too Large for FD=" + to_str(client_fd));
        send_error_response(client_fd, 413, "Payload Too Large", i);
        usleep(100000);
        return false;
    }

    return true;
}

size_t WebServer::find_header_end(const std::string &request_data)
{
    return request_data.find("\r\n\r\n");
}

int WebServer::parse_content_length(const std::string &headers)
{
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.find("Content-Length:") != std::string::npos)
        {
            std::istringstream linestream(line);
            std::string key;
            int content_length = 0;
            linestream >> key >> content_length;
            return content_length;
        }
    }
    return 0;
}

// --- Helper: Check if full body is received ---
bool WebServer::is_full_body_received(const Request &request, const std::string &request_data, size_t header_end)
{
    bool is_chunked = request.isChunked();
    int content_length = request.getContentLength();
    size_t body_start = header_end + 4;
    size_t body_length_received = request_data.size() - body_start;

    if (!is_chunked && content_length > 0 && body_length_received < static_cast<size_t>(content_length))
    {
        return false;
    }
    if (is_chunked && request.getBody().empty())
    {
        return false;
    }
    return true;
}

// ===== Helper functions for handleClientDataOn =====

// Helper: Read data from client socket
bool WebServer::readClientData(int client_fd, char* buf, size_t buf_size, ssize_t& bytes_read)
{
	bytes_read = ::read(client_fd, buf, buf_size);

	if (bytes_read == 0)
	{
		// Peer closed
		Logger::log(LOG_INFO, "WebServer", "FD=" + to_str(client_fd) + " EOF from peer; closing");
		closeClient(client_fd);
		return false;
	}

	if (bytes_read < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
		{
			Logger::log(LOG_DEBUG, "WebServer",
						"FD=" + to_str(client_fd) + " read would block/interrupt; returning");
			return false; // not fatal, try again later
		}
		Logger::log(LOG_ERROR, "WebServer",
					"FD=" + to_str(client_fd) + " read error errno=" + to_str(errno) + "; closing");
		closeClient(client_fd);
		return false;
	}

	return true; // successful read
}

// Helper: Check if buffer size is within limits
bool WebServer::validateBufferSize(int client_fd, size_t current_size, size_t new_bytes)
{
	if (current_size + new_bytes > 100 * 1024 * 1024) // 100MB limit
	{
		Logger::log(LOG_ERROR, "WebServer",
					"Client buffer too large, possible DoS. FD=" + to_str(client_fd));
		send_error_response(client_fd, 413, "Payload Too Large", 0);
		
		std::map<int, Connection>::iterator it = conns_.find(client_fd);
		if (it != conns_.end())
			it->second.shouldCloseAfterWrite = true;
		return false;
	}
	return true;
}

// Helper: Validate Content-Length against max body size
bool WebServer::validateContentLength(int client_fd, const std::string& headers)
{
	long maxBodySize = static_cast<long>(config_->getMaxBodySize());
	long len = parse_content_length(headers);
	
	if (len > maxBodySize)
	{
		Logger::log(LOG_ERROR, "WebServer",
					"FD=" + to_str(client_fd) + " declared Content-Length exceeds max body size");
		send_error_response(client_fd, 413, "Payload Too Large", 0);
		
		std::map<int, Connection>::iterator it = conns_.find(client_fd);
		if (it != conns_.end())
			it->second.shouldCloseAfterWrite = true;
		return false;
	}
	return true;
}

// Helper: Calculate how many bytes are needed for a complete request
size_t WebServer::calculateRequestSize(const std::string& buffer, size_t header_bytes, const std::string& headers)
{
	if (has_chunked_encoding(headers))
	{
		if (buffer.size() <= header_bytes)
			return 0; // need more data
		return buffer.size(); // hand over entire buffer for chunked parsing
	}
	else
	{
		long len = parse_content_length(headers);
		if (len < 0)
			len = 0;
		
		size_t total_needed = header_bytes + static_cast<size_t>(len);
		if (buffer.size() < total_needed)
			return 0; // need more data
		
		return total_needed;
	}
}

// Helper: Handle request parsing and execution
bool WebServer::processCompleteRequest(int client_fd, const std::string& frame)
{
	try
	{
		Request req(frame);
		process_request(req, client_fd, 0);
		return true;
	}
	catch (const std::exception &e)
	{
		std::string error_msg = e.what();
		Logger::log(LOG_ERROR, "WebServer", std::string("Request parse failed: ") + error_msg);

		// Determine appropriate error code
		int error_code = 400;
		std::string error_type = "Bad Request";
		
		if (error_msg.find("HTTP Version Not Supported") != std::string::npos)
		{
			error_code = 505;
			error_type = "HTTP Version Not Supported";
		}
		else if (error_msg.find("501:") == 0)
		{
			error_code = 501;
			error_type = "Not Implemented";
		}

		send_error_response(client_fd, error_code, error_type, 0);
		
		if (conns_.count(client_fd))
			conns_[client_fd].shouldCloseAfterWrite = true;
		
		return false;
	}
}

// Helper: Process all complete requests in the buffer
void WebServer::processBufferedRequests(int client_fd)
{
	for (;;)
	{
		// Connection might have been closed by processing
		std::map<int, Connection>::iterator it = conns_.find(client_fd);
		if (it == conns_.end())
			return;

		std::string &buffer = it->second.readBuf;

		// Do we have complete headers?
		size_t hdr_end = find_header_end(buffer);
		if (hdr_end == std::string::npos)
		{
			// Need more data to get headers
			return;
		}

		const size_t header_bytes = hdr_end + 4;
		std::string headers = buffer.substr(0, header_bytes);

		// Validate Content-Length
		if (!validateContentLength(client_fd, headers))
			return;

		// Calculate how many bytes we need for a complete request
		size_t needed = calculateRequestSize(buffer, header_bytes, headers);
		if (needed == 0)
		{
			// Need more data
			return;
		}

		// Validate buffer consistency
		if (needed > buffer.size())
		{
			Logger::log(LOG_ERROR, "WebServer",
						"Needed bytes exceed buffer size before parsing! FD=" + to_str(client_fd));
			send_error_response(client_fd, 400, "Bad Request", 0);
			it->second.shouldCloseAfterWrite = true;
			return;
		}

		// Extract complete request frame
		std::string frame = buffer.substr(0, needed);

		// Process the request
		if (!processCompleteRequest(client_fd, frame))
			return;

		// Re-fetch connection (might have been closed)
		std::map<int, Connection>::iterator it2 = conns_.find(client_fd);
		if (it2 == conns_.end())
			return;

		std::string &buffer2 = it2->second.readBuf;

		// Validate buffer state after processing
		if (needed > buffer2.size())
		{
			Logger::log(LOG_DEBUG, "WebServer",
						"Buffer smaller than expected after process_request; stopping. FD=" + to_str(client_fd));
			return;
		}

		// Consume processed request and continue with any remaining pipelined requests
		buffer2.erase(0, needed);
		if (buffer2.empty())
			return;
	}
}

// ===== Helper functions for process_request =====

// Helper: Setup connection policy based on HTTP version and Connection header
void WebServer::setupConnectionPolicy(Request& request, int client_fd)
{
	std::string ver = request.getVersion();
	std::string connHdr = request.getHeader("Connection");
	bool close_conn = (connHdr == "close") || (ver == "HTTP/1.0" && connHdr != "keep-alive");
	conns_[client_fd].shouldCloseAfterWrite = close_conn;

	Logger::log(LOG_INFO, "POLICY",
				"fd=" + to_str(client_fd) +
					" path=" + request.getPath() +
					" ver=" + ver +
					" conn=" + (connHdr.empty() ? std::string("<none>") : connHdr) +
					" closeAfter=" + (close_conn ? "true" : "false"));
}

// Helper: Perform basic request validation (method, body size, location permissions)
bool WebServer::performBasicValidation(Request& request, int client_fd, size_t i)
{
	std::string method = request.getMethod();
	std::string uri = request.getPath();
	const LocationConfig *loc = match_location(config_->getLocations(), uri);

	if (loc)
		Logger::log(LOG_DEBUG, "WebServer", "Matched location: " + loc->path);
	else
		Logger::log(LOG_DEBUG, "WebServer", "No location matched!");

	// Check if method is supported
	if (method != "GET" && method != "POST" && method != "DELETE")
	{
		send_error_response(client_fd, 501, "Not Implemented", i);
		return false;
	}

	// Check body size limits
	if (request.getContentLength() > static_cast<int>(config_->getMaxBodySize()))
	{
		send_error_response(client_fd, 413, "Payload Too Large", i);
		return false;
	}
	if (request.getBody().size() > config_->getMaxBodySize())
	{
		send_error_response(client_fd, 413, "Payload Too Large", i);
		return false;
	}

	// Check if method is allowed for this location
	if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end())
	{
		send_error_response(client_fd, 405, "Method Not Allowed", i);
		return false;
	}

	return true;
}

// Helper: Handle Expect: 100-continue header
bool WebServer::handleExpectContinue(Request& request, int client_fd, size_t i)
{
	if (request.hasExpectContinue())
	{
		if (!validate_post_request(request, client_fd, i))
			return false;
		
		Logger::log(LOG_INFO, "WebServer", "Client expects 100-continue, sending 100 Continue response");
		send_continue_response(client_fd);
	}
	return true;
}

// Helper: Handle CGI requests for GET/DELETE methods
bool WebServer::handleCGIRequest(Request& request, const LocationConfig* loc, int client_fd, size_t i)
{
	std::string method = request.getMethod();
	int is_cgi = (loc && !loc->cgi_extension.empty() && is_cgi_request(*loc, request.getPath())) ? 1 : 0;
	
	if ((method == "GET" || method == "DELETE") && loc && is_cgi)
	{
		Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
		handle_cgi(loc, request, client_fd, i);
		return true; // request handled
	}
	return false; // not a CGI request
}

// Helper: Handle location-based redirections
bool WebServer::handleRedirection(Request& request, const LocationConfig* loc, int client_fd, size_t i)
{
	if (loc && !loc->redirect_url.empty())
	{
		const std::string hostHdr = request.getHeader("Host");
		const bool external = isExternalRedirect(loc->redirect_url, hostHdr);
		
		if (external)
		{
			conns_[client_fd].shouldCloseAfterWrite = true;
			Logger::log(LOG_INFO, "redirect", "External → " + loc->redirect_url + " (will close)");
		}
		else
		{
			Logger::log(LOG_INFO, "redirect", "Internal → " + loc->redirect_url + " (keep-alive)");
		}
		
		send_redirect_response(client_fd, loc->redirect_code == 0 ? 301 : loc->redirect_code, loc->redirect_url, i);
		
		if (!conns_[client_fd].shouldCloseAfterWrite)
		{
			conns_[client_fd].readBuf.clear();
			Logger::log(LOG_DEBUG, "RESET", "fd=" + to_str(client_fd) + " cleared readBuf after internal redirect");
		}
		return true; // request handled
	}
	return false; // no redirection
}

// Helper: Dispatch to appropriate method handler
void WebServer::dispatchMethodHandler(Request& request, const LocationConfig* loc, int client_fd, size_t i)
{
	std::string method = request.getMethod();
	
	Logger::log(LOG_INFO, "request", "Ver=" + request.getVersion() + " ConnHdr=" + request.getHeader("Connection"));

	if (method == "GET")
	{
		handle_get(request, loc, client_fd, i);
	}
	else if (method == "POST")
	{
		if (!validate_post_request(request, client_fd, i))
			return;
		
		// Check CGI for POST
		int is_cgi = (loc && !loc->cgi_extension.empty() && is_cgi_request(*loc, request.getPath())) ? 1 : 0;
		if (loc && is_cgi)
		{
			Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
			handle_cgi(loc, request, client_fd, i);
			return;
		}
		
		Logger::log(LOG_DEBUG, "process_request",
					"POST " + request.getPath() +
						" matched to location " + (loc ? loc->path : "NULL") +
						" upload_dir=" + (loc ? loc->upload_dir : "<none>"));
		handle_post(request, loc, client_fd, i);
	}
	else if (method == "DELETE")
	{
		handle_delete(request, loc, client_fd, i);
	}
}

// Helper: Finalize request processing (cleanup for keep-alive connections)
void WebServer::finalizeRequestProcessing(int client_fd)
{
	if (!conns_[client_fd].shouldCloseAfterWrite)
	{
		conns_[client_fd].readBuf.clear();
		Logger::log(LOG_DEBUG, "RESET", "fd=" + to_str(client_fd) + " keeping alive; cleared readBuf");
	}
}
