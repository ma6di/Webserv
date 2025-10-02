# Webserv

A high-performance, C++98-compliant [HTTP/1.1](https://datatracker.ietf.org/doc/html/rfc9110#name-200-created) web server with CGI support, static file serving, and robust error handling. Designed for educational and practical use, it demonstrates low-level network programming, event-driven I/O, and HTTP protocol compliance.

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
6. **Stress Testing:**
   - We performed load testing on `Webserv` running locally on Linux using `siege`.
    ```bash
     siege -c 1000 -t 30S http://localhost:8080/```
    
{
  "transactions": 140109,
  "availability": 100.00,
  "elapsed_time": 29.14,
  "data_transferred": 3119.03,
  "response_time": 0.21,
  "transaction_rate": 4808.13,
  "throughput": 107.04,
  "concurrency": 992.92,
  "successful_transactions": 124264,
  "failed_transactions": 0,
  "longest_transaction": 1.16,
  "shortest_transaction": 0.00
}

Analysis:

- Webserv handled ~1,000 concurrent connections with 100% availability.

- Extremely low average response time: 0.21 ms.

- High throughput: 107 MB/sec.

- No failed transactions, demonstrating excellent stability under heavy load.

- Longest request: 1.16 ms — server remains highly responsive.


## Event-Driven Architecture: Restaurant Analogy

The server operates like a busy restaurant, efficiently handling many guests (clients) at once using non-blocking I/O and the `poll()` system call. Here’s how the flow works:

```
+--------------------------------+
| socket() -> bind() -> listen() |  <-- Open restaurant & doors
+--------------------------------+
              |
              v
   +-----------------------+
   |  Add listening socket |
   |  to pollfd[] array    |
   +-----------------------+
              |
              v
        +-------------+
        | poll() waits|  <-- Waiter watching doors & tables
        +-------------+
              |
              v
    +---------+------------------+
    |                            |
Listening socket?           Client socket?
    |                            |
  POLLIN?              POLLIN?   |    POLLOUT?
    |                     |  ----------  |
    v                     v              v
+----------+        +-----------+   +-----------+
| accept() |        |   read()  |   |  write()  |
+----------+        |  request  |   | response  |
    |               +-----------+   +-----------+
    v                     |               |
Add new client_fd         |               |
to pollfd[] array         v               |
    |               +------------------+  |
    +-------------->| Process request  |  |
                    | (parse, route,   |  |
                    |  generate resp)  |  |
                    +------------------+  |
                              |           |
                              v           |
                    Queue response in     |
                    connection buffer     |
                              |           |
                              +-----------+
                                          |
                              Connection closed? 
                           (EOF/error/keep-alive timeout)
                                          |
                                          v
                             +----------------------------+
                             | close(fd), remove from     |
                             | pollfd[] array (guest left)|
                             +----------------------------+
                                           |
                                           v
                                      Loop forever
```

**Explanation:**
- The server opens its "doors" (`socket()`, `bind()`, `listen()`) and adds the entrance to a list of things to watch (`pollfd[]`).
- Like a waiter, `poll()` keeps an eye on both the entrance (new guests) and all tables (active clients).
- When a new guest arrives, the server "seats" them (`accept()`) and starts watching their table (client socket).
- For each client:
  - If they want to order (`POLLIN`), the server reads their request.
  - If their food (response) is ready (`POLLOUT`), the server serves it.
- Requests are parsed, routed, and responses are generated and queued.
- When a guest leaves (connection closes), their table is cleared (`close(fd)`), and the server keeps running, ready for more.

This architecture allows the server to handle thousands of connections efficiently, just like a well-run restaurant with a vigilant staff.

---

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
