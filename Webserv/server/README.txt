# Module: Core HTTP Server

## Purpose

- Set up listening sockets on configured ports
- Run the event loop using `poll()`
- Accept new client connections
- Read and parse HTTP requests
- Match requests to locations and handlers (static, upload, CGI, etc.)
- Write HTTP responses to clients

## Components

- `WebServer.hpp` / `WebServer.cpp`
- Handler implementations in `methodHandlers.cpp`
- Helpers in `serverUtils.cpp`, `sendResponse.cpp`

## Responsibilities

- Bind to ports (from config)
- Accept and manage clients
- Read data and parse into `Request`
- Match URI to `LocationConfig`
- Delegate to static file, upload, or CGI handler
- Handle errors and send appropriate HTTP responses

## Integration

- Central dispatcher for all modules
- Uses `Config`, `Request`, `CGIHandler`, and `utils`
- Calls helpers for file I/O, MIME types, and directory listings

---

## Key Concepts Summary

| Concept             | Description                                            |
| ------------------- | ------------------------------------------------------ |
| `poll()`            | Monitors multiple sockets for I/O                      |
| `non-blocking`      | Prevents blocking a single client from stalling server |
| `request parser`    | Parses incoming HTTP requests                          |
| `method validation` | Only allows GET/POST/DELETE based on config            |
| `CGIHandler`        | Forks a subprocess and reads back script output        |
| `static file serve` | Reads file and wraps it in an HTTP response            |

---

### About `poll()`

`poll()` is a system call that lets you monitor multiple file descriptors (sockets, pipes, etc.) to see if they are ready for reading, writing, or have an error.  
It’s part of the POSIX standard and is available on all Unix-like systems.

---

### About `pollfd`

`pollfd` is a structure defined in `<poll.h>` used with the `poll()` system call.  
It describes which file descriptor you want to monitor, what events you care about, and what actually happened.

**Structure:**
```cpp
struct pollfd {
    int   fd;       // File descriptor to monitor
    short events;   // Events to look for (input/output/error)
    short revents;  // Events that actually occurred (filled by poll())
};
```