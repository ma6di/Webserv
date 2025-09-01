#include "WebServer.hpp"
#include "Config.hpp"
#include <cstring>
#include <ctime>
#include <errno.h>

WebServer::WebServer(const Config &cfg)
	: config_(&cfg)
{
	std::vector<int> ports = config_->getPorts();
	std::vector<std::string> hosts = config_->getHosts();

	if (hosts.size() < ports.size())
		hosts.resize(ports.size(), std::string());

	for (size_t idx = 0; idx < ports.size(); ++idx)
	{
		int port = ports[idx];
		const std::string &host = hosts[idx];

		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
		{
			perror("socket");
			continue;
		}

		make_socket_non_blocking(sock);

		int opt = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			perror("setsockopt");
			// close(sock);
			continue;
		}

		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);

		in_addr resolved;
		if (!host.empty() && resolve_ipv4(host, &resolved))
		{
			addr.sin_addr = resolved;
		}
		else
		{
			addr.sin_addr.s_addr = INADDR_ANY;
		}

		if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
		{
			perror("bind");
			close(sock);
			continue;
		}

		if (listen(sock, SOMAXCONN) < 0)
		{
			perror("listen");
			close(sock);
			continue;
		}

		Logger::log(LOG_INFO, "WebServer",
					"Server listening on http://" + (host.empty() ? std::string("0.0.0.0") : host) + ":" + to_str(port));
		listening_sockets.push_back(sock);
	}
}

WebServer::~WebServer()
{
	shutdown();
}

void WebServer::shutdown()
{
	for (size_t i = 0; i < listening_sockets.size(); ++i)
	{
		::close(listening_sockets[i]);
	}
	listening_sockets.clear();

	for (std::map<int, Connection>::iterator it = conns_.begin();
		 it != conns_.end(); ++it)
	{
		::close(it->first);
	}
	conns_.clear();

	Logger::log(LOG_INFO, "WebServer", "All sockets closed.");
}

void WebServer::make_socket_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("Failed to make socket non-blocking");
}

int WebServer::handleNewConnection(int listen_fd)
{
	sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	int client_fd = accept(listen_fd, (sockaddr *)&client_addr, &addrlen);
	if (client_fd < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			perror("accept");
		return -1;
	}

	make_socket_non_blocking(client_fd);
	conns_[client_fd]; // Create new connection (last_active already set in constructor)
	Logger::log(LOG_INFO, "WebServer", "Accepted FD=" + to_str(client_fd));
	return client_fd;
}

void WebServer::handleClientDataOn(int client_fd)
{
	// --- Single-shot read per POLLIN ---
	char buf[4096];
	ssize_t n = ::read(client_fd, buf, sizeof(buf));

	if (n == 0)
	{
		// Peer closed
		Logger::log(LOG_INFO, "WebServer", "FD=" + to_str(client_fd) + " EOF from peer; closing");
		closeClient(client_fd); // use your unified close; fallback to cleanup_client if needed
		return;
	}

	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
		{
			Logger::log(LOG_DEBUG, "WebServer",
						"FD=" + to_str(client_fd) + " read would block/interrupt; returning");
			return; // not fatal
		}
		Logger::log(LOG_ERROR, "WebServer",
					"FD=" + to_str(client_fd) + " read error errno=" + to_str(errno) + "; closing");
		closeClient(client_fd);
		return;
	}

	// n > 0
	updateClientActivity(client_fd);

	// Find connection
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return;

	std::string &data = it->second.readBuf;

	// Optional guard against runaway buffer growth
	if (data.size() + static_cast<size_t>(n) > 100 * 1024 * 1024) // total 104,857,600  tester 204,800,000
	{
		Logger::log(LOG_ERROR, "WebServer",
					"Client buffer too large, possible DoS. FD=" + to_str(client_fd));
		// Enqueue 413 and close-after-write instead of immediate close
		send_error_response(client_fd, 413, "Payload Too Large", 0);
		it->second.shouldCloseAfterWrite = true;
		return;
	}

	// Append read bytes
	data.append(buf, static_cast<size_t>(n));
	Logger::log(LOG_DEBUG, "WebServer",
				"FD=" + to_str(client_fd) + " buffer size after append: " + to_str(data.size()));

	// --- Immediate error if buffer exceeds max body size ---
	long maxBodySize = static_cast<long>(config_->getMaxBodySize());
	if (data.size() > static_cast<size_t>(maxBodySize)) {
		Logger::log(LOG_ERROR, "WebServer",
					"FD=" + to_str(client_fd) + " buffer exceeds max body size");
		send_error_response(client_fd, 413, "Payload Too Large", 0);
		return;
	}

	// --- Handle pipelined requests present in the buffer (no more reads here) ---
	for (;;)
	{
		// Connection might have been closed by processing
		std::map<int, Connection>::iterator it2 = conns_.find(client_fd);
		if (it2 == conns_.end())
			return;

		std::string &buffer = it2->second.readBuf;

		// Do we have full headers?
		size_t hdr_end = find_header_end(buffer);
		if (hdr_end == std::string::npos) {
			// Need more data to get headers
			return;
		}

		const size_t header_bytes = hdr_end + 4;
		std::string headers = buffer.substr(0, header_bytes);

		// Header protocol checks
		// int headerErr = check_headers(headers, maxBodySize);
		// if (headerErr) {
		// 	int code = headerErr == 413 ? 413 : (headerErr == 501 ? 501 : 400);
		// 	const char *msg = code == 413 ? "Payload Too Large" : (code == 501 ? "Not Implemented" : "Bad Request");
		// 	Logger::log(LOG_ERROR, "WebServer", "FD=" + to_str(client_fd) + " header error, code=" + to_str(code));
		// 	send_error_response(client_fd, code, msg, 0);
		// 	it2->second.shouldCloseAfterWrite = true;
		// 	return;
		// }

		// --- Immediate error if declared Content-Length exceeds max body size ---
		long len = parse_content_length(headers);
		if (len > maxBodySize) {
			Logger::log(LOG_ERROR, "WebServer",
						"FD=" + to_str(client_fd) + " declared Content-Length exceeds max body size");
			send_error_response(client_fd, 413, "Payload Too Large", 0);
			it2->second.shouldCloseAfterWrite = true;
			return;
		}

		// Decide how many bytes constitute one full request
		size_t needed = header_bytes;
		if (has_chunked_encoding(headers))
		{
			// We don’t know yet; wait until the request parser can tell us the full size
			// If your Request class can parse chunked fully from the buffer, you may let it do so.
			if (buffer.size() <= header_bytes)
				return;
			// Hand over entire buffer to Request for parsing of chunked; it will validate completeness.
			needed = buffer.size();
		}
		else
		{
			if (len < 0)
				len = 0;
			if (buffer.size() < header_bytes + static_cast<size_t>(len))
			{
				// Wait for the rest of the body
				return;
			}
			needed = header_bytes + static_cast<size_t>(len);
		}

		// --- Immediate error if buffer exceeds declared Content-Length ---
		if (len > 0 && buffer.size() > header_bytes + static_cast<size_t>(len)) {
			Logger::log(LOG_ERROR, "WebServer",
						"FD=" + to_str(client_fd) + " buffer exceeds declared Content-Length");
			send_error_response(client_fd, 400, "Bad Request", 0);
			it2->second.shouldCloseAfterWrite = true;
			return;
		}

		if (needed > buffer.size())
		{
			// Shouldn’t happen with the checks above; treat as protocol error
			Logger::log(LOG_ERROR, "WebServer",
						"Needed bytes exceed buffer size before parsing! FD=" + to_str(client_fd));
			send_error_response(client_fd, 400, "Bad Request", 0);
			it2->second.shouldCloseAfterWrite = true;
			return;
		}

		// Slice exactly one complete request frame
		std::string frame = buffer.substr(0, needed);

		try
		{
			std::cout << "in the try/catch req before making req object\n";
			Request req(frame);
			std::cout << "in the try/catch req \n";
			process_request(req, client_fd, 0); // MUST enqueue response only; no direct write()
		}
		catch (const std::exception &e)
		{
			std::string error_msg = e.what();
			Logger::log(LOG_ERROR, "WebServer", std::string("Request parse failed: ") + error_msg);

			// Enqueue an error response; DO NOT flush() here; let POLLOUT handle it
			if (error_msg.find("HTTP Version Not Supported") != std::string::npos)
			{
				send_error_response(client_fd, 505, "HTTP Version Not Supported", 0);
				if (conns_.count(client_fd))
					conns_[client_fd].shouldCloseAfterWrite = true;
			}
			else if (error_msg.find("501:") == 0)
			{
				send_error_response(client_fd, 501, "Not Implemented", 0);
				if (conns_.count(client_fd))
					conns_[client_fd].shouldCloseAfterWrite = true;
			}
			else
			{
				send_error_response(client_fd, 400, "Bad Request", 0);
				if (conns_.count(client_fd))
					conns_[client_fd].shouldCloseAfterWrite = true;
			}
		}

		// Re-fetch connection and buffer (process_request may have queued output or closed)
		std::map<int, Connection>::iterator it3 = conns_.find(client_fd);
		if (it3 == conns_.end())
			return;

		std::string &buffer2 = it3->second.readBuf;

		if (needed > buffer2.size())
		{
			// Buffer changed unexpectedly (e.g., connection state changed). Stop here.
			Logger::log(LOG_DEBUG, "WebServer",
						"Buffer smaller than expected after process_request; stopping. FD=" + to_str(client_fd));
			return;
		}

		// Consume exactly one frame and continue loop for any pipelined requests still buffered
		buffer2.erase(0, needed);
		if (buffer2.empty())
		{
			return;
		}
	}
}

// --- Helper: Process the request ---
void WebServer::process_request(Request &request, int client_fd, size_t i)
{
	std::cout << "method: " << request.getMethod() << "\n";
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

	std::string method = request.getMethod();
	std::string uri = request.getPath();
	const LocationConfig *loc = match_location(config_->getLocations(), uri);

	if (loc)
		Logger::log(LOG_DEBUG, "WebServer", "Matched location: " + loc->path);
	else
		Logger::log(LOG_DEBUG, "WebServer", "No location matched!");

	// General error: 501 Not Implemented
	if (method != "GET" && method != "POST" && method != "DELETE")
	{
		send_error_response(client_fd, 501, "Not Implemented", i);
		return;
	}

	// General error: 413 Payload Too Large
	if (request.getContentLength() > static_cast<int>(config_->getMaxBodySize()))
	{
		send_error_response(client_fd, 413, "Payload Too Large", i);
		return;
	}
	if (request.getBody().size() > config_->getMaxBodySize())
	{
		send_error_response(client_fd, 413, "Payload Too Large", i);
		return;
	}

	// General error: 405 Method Not Allowed
	if (loc && std::find(loc->allowed_methods.begin(), loc->allowed_methods.end(), method) == loc->allowed_methods.end())
	{
		send_error_response(client_fd, 405, "Method Not Allowed", i);
		return;
	}
	// Handle Expect: 100-continue header
	if (request.hasExpectContinue())
	{
		if (!validate_post_request(request, client_fd, i))
		{
			// send_error_response(client_fd, 413, "Payload Too Large", i);
			// conns_[client_fd].shouldCloseAfterWrite = true;
			return;
		}
		Logger::log(LOG_INFO, "WebServer", "Client expects 100-continue, sending 100 Continue response");
		send_continue_response(client_fd);
	}

	// CGI check for GET/DELETE (not POST)
	int is_cgi = (loc && !loc->cgi_extension.empty() && is_cgi_request(*loc, request.getPath())) ? 1 : 0;
	if ((method == "GET" || method == "DELETE") && loc && is_cgi)
	{
		Logger::log(LOG_DEBUG, "WebServer", "is_cgi_request: " + to_str(is_cgi));
		handle_cgi(loc, request, client_fd, i);
		return;
	}

	// Redirect check
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
		return;
	}

	Logger::log(LOG_INFO, "request", "Ver=" + request.getVersion() + " ConnHdr=" + request.getHeader("Connection"));

	// Method-specific error checks and handling
	if (method == "GET")
	{
		handle_get(request, loc, client_fd, i);
	}
	else if (method == "POST")
	{
		if (!validate_post_request(request, client_fd, i))
		{
			return;
		}
		// CGI check for POST
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
	if (!conns_[client_fd].shouldCloseAfterWrite)
	{
		conns_[client_fd].readBuf.clear();
		Logger::log(LOG_DEBUG, "RESET", "fd=" + to_str(client_fd) + " keeping alive; cleared readBuf");
	}
}

// Helper for POST validation
/*bool WebServer::validate_post_request(Request &request, int client_fd, size_t i)
{
	long contentLength = request.getContentLength();
	bool isChunked = request.isChunked();
	long maxBodySize = config_->getMaxBodySize();

	// Must have either Content-Length or Transfer-Encoding: chunked, but not both
	if (contentLength && isChunked)
	{
		send_error_response(client_fd, 400, "Bad Request", i);
		flushPendingWrites(client_fd);
		return false;
	}
	if (!contentLength && !isChunked)
	{
		send_error_response(client_fd, 411, "Length Required", i);
		// flushPendingWrites(client_fd);
		return false;
	}
	// If Content-Length, check for negative, too large, or mismatch
	if (contentLength)
	{
		if (contentLength < 0 || contentLength > maxBodySize)
		{
			send_error_response(client_fd, 413, "Payload Too Large", i);
			// flushPendingWrites(client_fd);
			return false;
		}
		if (contentLength != static_cast<long>(request.getBody().size()))
		{
			send_error_response(client_fd, 400, "Bad Request", i);
			// flushPendingWrites(client_fd);
			return false;
		}
	}
	// If chunked, optionally check for support
	// if (isChunked && !server_supports_chunked) {
	//     send_error_response(client_fd, 501, "Not Implemented", i);
	//     flushPendingWrites(client_fd);
	//     return false;
	// }
	return true;
}*/

bool WebServer::validate_post_request(Request &request, int client_fd, size_t i)
{
	long contentLength = request.getContentLength();
	bool isChunked = request.isChunked();
	long maxBodySize = config_->getMaxBodySize();

	std::cout << "max body size: " << maxBodySize << "\n";
	std::cout << "content lenght: " << contentLength << "\n";
	// Must have either Content-Length or chunked, but not both
	if (contentLength && isChunked)
	{
		send_error_response(client_fd, 400, "Bad Request", i);
		if (conns_.count(client_fd))
			conns_[client_fd].shouldCloseAfterWrite = true;
		return false;
	}
	if (!contentLength && !isChunked)
	{
		send_error_response(client_fd, 411, "Length Required", i);
		if (conns_.count(client_fd))
			conns_[client_fd].shouldCloseAfterWrite = true;
		return false;
	}

	// Content-Length checks
	if (contentLength)
	{
		if (contentLength < 0 || contentLength > maxBodySize)
		{
			send_error_response(client_fd, 413, "Payload Too Large", i);
			if (conns_.count(client_fd))
				conns_[client_fd].shouldCloseAfterWrite = true;
			return false;
		}
		if (contentLength != static_cast<long>(request.getBody().size()))
		{
			send_error_response(client_fd, 400, "Bad Request", i);
			if (conns_.count(client_fd))
				conns_[client_fd].shouldCloseAfterWrite = true;
			return false;
		}
	}

	// ok
	return true;
}

void WebServer::cleanup_client(int client_fd, int i)
{
	(void)i;
	::close(client_fd);
	conns_.erase(client_fd);
	Logger::log(LOG_INFO, "WebServer", "Cleaned up client FD=" + to_str(client_fd));
}

void WebServer::handle_new_connection(int listen_fd)
{
	handleNewConnection(listen_fd);
}

void WebServer::queueResponse(int client_fd,
							  const std::string &rawResponse)
{
	Connection &conn = conns_[client_fd];
	conn.writeBuf += rawResponse;
}

bool WebServer::hasPendingWrite(int client_fd) const
{
	std::map<int, Connection>::const_iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return false;
	return !it->second.writeBuf.empty();
}

/*void WebServer::flushPendingWrites(int client_fd)
{
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return;
	Connection &conn = it->second;

	while (!conn.writeBuf.empty())
	{
		ssize_t n = ::write(client_fd,
							conn.writeBuf.data(),
							conn.writeBuf.size());
		if (n > 0)
		{
			conn.writeBuf.erase(0, static_cast<size_t>(n));
			// Update activity timestamp when we successfully write data
			updateClientActivity(client_fd);
		}
		else if (n == 0)
		{
			break; // should not happen
		}
		else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		{
			break;
		}
		else
		{
			cleanup_client(client_fd, 0);
			return;
		}
	}

	if (conn.writeBuf.empty() && conn.shouldCloseAfterWrite)
	{
		Logger::log(LOG_DEBUG, "flush",
					"fd=" + to_str(client_fd) + " drained; closing");
		::shutdown(client_fd, SHUT_WR);
		cleanup_client(client_fd, 0);
	}
	else
	{
		Logger::log(LOG_DEBUG, "flush", "fd=" + to_str(client_fd) + " drained; keeping open");
	}
}*/

void WebServer::flushPendingWrites(int client_fd)
{
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return;

	Connection &conn = it->second;

	// Nothing to send? Let the next buildPollFds() omit POLLOUT.
	if (conn.writeBuf.empty())
		return;

	// EXACTLY ONE write() attempt per POLLOUT event
	ssize_t n = ::write(client_fd, conn.writeBuf.data(), conn.writeBuf.size());

	if (n > 0)
	{
		// Consume written bytes
		conn.writeBuf.erase(0, static_cast<size_t>(n));
		updateClientActivity(client_fd);

		// If we fully drained the buffer, decide whether to close
		if (conn.writeBuf.empty())
		{
			if (conn.shouldCloseAfterWrite)
			{
				// Close now; next poll build won’t include this fd
				closeClient(client_fd); // or cleanup_client(client_fd, 0);
				return;
			}
			// Drained but keeping open (keep-alive). No POLLOUT next time.
			Logger::log(LOG_DEBUG, "flush", "fd=" + to_str(client_fd) + " drained; keeping open");
		}
		return; // one write done this cycle
	}

	if (n == 0)
	{
		// Peer closed the write side; remove client.
		closeClient(client_fd); // or cleanup_client(client_fd, 0);
		return;
	}

	// n < 0: only look at errno because write() failed
	if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	{
		// Not fatal; try again on next POLLOUT
		return;
	}

	// Any other error => drop the client
	closeClient(client_fd); // or cleanup_client(client_fd, 0);
}

// Timeout management methods
time_t WebServer::getClientLastActive(int client_fd) const
{
	std::map<int, Connection>::const_iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return 0;
	return it->second.last_active;
}

void WebServer::updateClientActivity(int client_fd)
{
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it != conns_.end())
	{
		it->second.last_active = time(NULL);
	}
}

void WebServer::closeClient(int client_fd)
{
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it != conns_.end())
	{
		::close(client_fd);
		conns_.erase(it);
		Logger::log(LOG_INFO, "timeout", "Closed client fd=" + to_str(client_fd) + " due to timeout");
	}
}

bool WebServer::isAbsoluteHttpUrl(const std::string &s)
{
	return (s.compare(0, 7, "http://") == 0) ||
		   (s.compare(0, 8, "https://") == 0) ||
		   (s.compare(0, 2, "//") == 0);
}

std::string WebServer::hostportFromUrl(const std::string &url)
{

	std::string s = url;
	size_t start = 0;
	if (s.compare(0, 2, "//") == 0)
	{
		start = 2;
	}
	else
	{
		size_t p = s.find("://");
		if (p != std::string::npos)
			start = p + 3;
	}
	size_t end = s.find('/', start);
	if (end == std::string::npos)
		end = s.size();
	return s.substr(start, end - start);
}

bool WebServer::iequals(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		char ca = a[i], cb = b[i];
		if ('A' <= ca && ca <= 'Z')
			ca = char(ca - 'A' + 'a');
		if ('A' <= cb && cb <= 'Z')
			cb = char(cb - 'A' + 'a');
		if (ca != cb)
			return false;
	}
	return true;
}

bool WebServer::isExternalRedirect(const std::string &location, const std::string &reqHost)
{
	if (!location.empty() && location[0] == '/')
		return false;

	if (isAbsoluteHttpUrl(location))
	{
		std::string hp = hostportFromUrl(location);
		if (reqHost.empty())
			return true;
		return !iequals(hp, reqHost);
	}

	return true;
}

bool WebServer::resolve_ipv4(const std::string &host, in_addr *out)
{
	if (host.empty())
		return false; // caller will handle as INADDR_ANY

	// Treat 0.0.0.0 explicitly as ANY
	if (host == "0.0.0.0")
	{
		out->s_addr = INADDR_ANY;
		return true;
	}

	if (host == "localhost")
	{
		// 127.0.0.1
		if (inet_aton("127.0.0.1", out) != 0)
			return true;
		return false;
	}

	// Try dotted-quad first
	if (inet_aton(host.c_str(), out) != 0)
		return true;

	// DNS fallback (IPv4 only, C++98-friendly)
	struct hostent *he = gethostbyname(host.c_str());
	if (!he || he->h_addrtype != AF_INET || he->h_length != (int)sizeof(in_addr))
		return false;

	std::memcpy(out, he->h_addr_list[0], sizeof(in_addr));
	return true;
}

void WebServer::markCloseAfterWrite(int fd)
{
	std::map<int, Connection>::iterator it = conns_.find(fd);
	if (it != conns_.end())
		it->second.shouldCloseAfterWrite = true;
}

// // Helper: Checks headers for protocol errors, returns error code or 0 if OK
// int WebServer::check_headers(const std::string &headers, long maxBodySize) {
//     int contentLengthCount = 0;
//     long contentLength = -1;
//     bool hasTransferEncoding = false;
//     bool hasHost = false;
//     std::string version;
//     std::string method;

//     // Simple line-by-line parse
//     size_t pos = 0;
//     bool firstLine = true;
//     while (pos < headers.size()) {
//         size_t end = headers.find("\r\n", pos);
//         if (end == std::string::npos) break;
//         std::string line = headers.substr(pos, end - pos);
//         pos = end + 2;
//         if (line.empty()) continue;
//         if (firstLine) {
//             // Parse request line: METHOD PATH VERSION
//             size_t m1 = line.find(' ');
//             size_t m2 = line.find(' ', m1 + 1);
//             if (m1 != std::string::npos && m2 != std::string::npos) {
//                 method = line.substr(0, m1);
//                 version = line.substr(m2 + 1);
//             }
//             firstLine = false;
//             continue;
//         }
//         size_t colon = line.find(':');
//         if (colon == std::string::npos) continue;
//         std::string key = line.substr(0, colon);
//         std::string value = line.substr(colon + 1);
//         // Trim
//         while (!key.empty() && (key[0] == ' ' || key[0] == '\t')) key.erase(0, 1);
//         while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
//         // Lowercase for comparison
//         for (size_t i = 0; i < key.size(); ++i) key[i] = tolower(key[i]);
//         for (size_t i = 0; i < value.size(); ++i) value[i] = tolower(value[i]);
//         if (key == "content-length") {
//             contentLengthCount++;
//             char *endptr = NULL;
//             long cl = strtol(value.c_str(), &endptr, 10);
//             if (*endptr != '\0') return 400; // Bad Request: non-numeric CL
//             contentLength = cl;
//         }
//         if (key == "transfer-encoding") {
//             hasTransferEncoding = true;
//             if (value.find("chunked") == std::string::npos) return 501; // Not Implemented: unsupported encoding
//         }
//         if (key == "host") hasHost = true;
//         // Check for invalid header chars (simple)
//         for (size_t i = 0; i < key.size(); ++i) {
//             if (key[i] < 32 || key[i] == 127) return 400;
//         }
//     }
//     // Only enforce Host for HTTP/1.1
//     if (version == "HTTP/1.1" && !hasHost) return 400;
//     // Only check body-related headers for POST/PUT
//     if (method == "POST" || method == "PUT") {
//         if (contentLengthCount > 1) return 400;
//         if (contentLength < 0) return 400;
//         if (contentLengthCount && hasTransferEncoding) return 400;
//         if (contentLength > maxBodySize) return 413;
//     }
//     // For other methods, ignore Content-Length/Transfer-Encoding unless present and invalid
//     if ((method != "POST" && method != "PUT") && (contentLengthCount > 1 || contentLength < 0)) return 400;
//     return 0;
// }
