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
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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
		if (!host.empty() && resolve_ipv4(host, &resolved)) {
			addr.sin_addr = resolved;
		}
		else {
			addr.sin_addr.s_addr = INADDR_ANY;
		}

		if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
			perror("bind");
			close(sock);
			continue;
		}

		if (listen(sock, SOMAXCONN) < 0) {
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
		return -1;

	make_socket_non_blocking(client_fd);
	conns_[client_fd]; // Create new connection (last_active already set in constructor)
	Logger::log(LOG_INFO, "WebServer", "Accepted FD=" + to_str(client_fd));
	return client_fd;
}

void WebServer::handleClientDataOn(int client_fd)
{
	// Read data from socket
	char buf[4096];
	ssize_t bytes_read;
	
	if (!readClientData(client_fd, buf, sizeof(buf), bytes_read))
		return;

	// Update client activity
	updateClientActivity(client_fd);

	// Find connection
	std::map<int, Connection>::iterator it = conns_.find(client_fd);
	if (it == conns_.end())
		return;

	std::string &data = it->second.readBuf;

	// Validate buffer size limits
	if (!validateBufferSize(client_fd, data.size(), static_cast<size_t>(bytes_read)))
		return;

	// Append new data to buffer
	data.append(buf, static_cast<size_t>(bytes_read));
	//Logger::log(LOG_DEBUG, "WebServer",
				//"FD=" + to_str(client_fd) + " buffer size after append: " + to_str(data.size()));

	// Process all complete requests in buffer
	processBufferedRequests(client_fd);
}

// --- Main request processing function (now modular) ---
void WebServer::process_request(Request &request, int client_fd, size_t i)
{
	// Setup connection policy
	setupConnectionPolicy(request, client_fd);

	// Perform basic validation
	if (!performBasicValidation(request, client_fd, i))
		return;

	// Handle Expect: 100-continue
	if (!handleExpectContinue(request, client_fd, i))
		return;

	std::string uri = request.getPath();
	const LocationConfig *loc = match_location(config_->getLocations(), uri);

	// Handle CGI requests for GET/DELETE
	if (handleCGIRequest(request, loc, client_fd, i))
		return;

	// Handle redirections
	if (handleRedirection(request, loc, client_fd, i))
		return;

	// Dispatch to method-specific handlers
	dispatchMethodHandler(request, loc, client_fd, i);

	// Finalize request processing
	finalizeRequestProcessing(client_fd);
}

bool WebServer::validate_post_request(Request &request, int client_fd, size_t i)
{
	long contentLength = request.getContentLength();
	bool isChunked = request.isChunked();
	long maxBodySize = config_->getMaxBodySize();

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
			//Logger::log(LOG_DEBUG, "flush", "fd=" + to_str(client_fd) + " drained; keeping open");
		}
		return; // one write done this cycle
	}

	if (n == 0)
	{
		// Peer closed the write side; remove client.
		closeClient(client_fd); // or cleanup_client(client_fd, 0);
		return;
	}

	return;
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
