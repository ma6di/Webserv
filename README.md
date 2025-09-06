# Webserv

A high-performance, C++98-compliant HTTP/1.1 web server with CGI support, static file serving, and robust error handling. Designed for educational and practical use, it demonstrates low-level network programming, event-driven I/O, and HTTP protocol compliance.

## Features

- **HTTP/1.1 Support:** Handles persistent connections, pipelining, chunked transfer encoding, and compliant header parsing.
- **Event-driven Architecture:** Uses `poll()` for scalable, non-blocking I/O.
- **Configurable:** Reads server and location configuration from `.conf` files.
- **Static File Serving:** Serves files and directories from the `www/` root.
- **CGI Execution:** Runs Python, PHP, and other CGI scripts from `www/cgi-bin/`.
- **Robust Error Handling:** Returns correct HTTP status codes for malformed requests, unsupported methods, timeouts, and more.
- **Logging:** Color-coded, level-based logging for debugging and monitoring.
- **Automated Test Suite:** `test_all.sh` runs comprehensive tests for all major features and edge cases.

## Directory Structure

- `Webserv/`
  - `main.cpp` — Entry point, server startup
  - `server/` — Core server logic (event loop, request handling, response sending)
  - `Request_Response/` — HTTP request/response parsing and construction
  - `config/` — Configuration parsing and management
  - `cgi/` — CGI handler and helpers
  - `logger/` — Logging utilities
  - `utils/` — Miscellaneous helpers
  - `www/` — Web root (static files, CGI scripts, error pages, uploads)
  - `test_all.sh` — Automated test script
  - `Makefile` — Build instructions

## How It Works

1. **Startup:**
   - Reads configuration from `default.conf` (or other `.conf` files).
   - Binds to configured ports and hosts.
   - Enters the event loop, waiting for client connections.

2. **Request Handling:**
   - Accepts new connections, reads data into per-client buffers.
   - Detects full HTTP requests (headers + body) using mini-parsing.
   - Validates headers and request framing before passing to the main parser.
   - Handles GET, POST, DELETE, and CGI requests according to config and HTTP/1.1 rules.

3. **CGI Support:**
   - Forwards request body and environment to CGI scripts via pipes.
   - Reads CGI output, parses headers/body, and returns to the client.

4. **Error Handling:**
   - Returns appropriate status codes for protocol errors, timeouts, unsupported features, and resource issues.

5. **Testing:**
   - Run `./test_all.sh` to execute a full suite of HTTP, CGI, and edge-case tests. Results are logged in `test_results.log`.

## Building & Running

```
cd Webserv
make
./webserv [config_file]
```
- Default config: `default.conf`
- Web root: `www/`

## Configuration
- See `Webserv/default.conf` for example configuration.
- Supports multiple servers, locations, allowed methods, CGI extensions, upload directories, and error pages.

## Testing
- Run `./test_all.sh` from the `Webserv/` directory.
- Requires `curl`, `nc`, and Python for CGI tests.
- Results are color-coded and logged to `test_results.log`.

## Accessing the Front Website

- The main website is served from the `www/` directory. Open your browser and go to:
  - `http://localhost:8080/` (or the port you configured)
- If the server is started with `host` set to `0.0.0.0`, it will listen on all network interfaces. You can access the website from any device on your local network using the server's IP address.
- To find your local IP, run `ping <hostname>` or `ifconfig`/`ipconfig` and use the reported address:
  - Example: `http://192.168.1.10:8080/`

## Notable Files
- `Webserv/server/WebServer.cpp`: Main event loop, connection and request management.
- `Webserv/Request_Response/Request.cpp`: HTTP request parsing and validation.
- `Webserv/cgi/CGIHandler.cpp`: CGI process management and I/O.
- `Webserv/test_all.sh`: Automated test suite.

## Authors
- [Mahdi (@ma6di)](https://github.com/ma6di)
- [Tayfun (tayfunkas)](https://github.com/tayfunkas)
- [Jess (JSlusark)](https://github.com/JSlusark)

## License
MIT License (see LICENSE file if present)

---

For more details, see code comments and the test suite for usage examples and edge case handling.