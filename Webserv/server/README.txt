
**Module:** Core HTTP Server

**Purpose:**
	Setting up the listening socket
	Running the event loop using poll()
	Accepting new connections
	Reading/parsing HTTP requests
	Dispatching either static file serving or CGI execution
	Writing the response to the clien

**Components:**

* `WebServer.hpp` / `WebServer.cpp`

**Responsibilities:**

* Bind to port (`listen` from config).
* Accept new clients.
* Read data, parse into `Request`.
* Match URI using config.
* Delegate to CGI or static handler.

**Integration:**

* Central dispatcher for all modules.
* Uses `Config`, `Request`, `CGIHandler`, and `utils`.

-----------
Key Concepts Summary

| Concept             | Description                                            |
| ------------------- | ------------------------------------------------------ |
| `poll()`            | Monitors multiple sockets for I/O                      |
| `non-blocking`      | Prevents blocking a single client from stalling server |
| `request parser`    | Parses incoming HTTP requests                          |
| `method validation` | Only allows GET/POST based on config                   |
| `CGIHandler`        | Forks a subprocess and reads back script output        |
| `static file serve` | Reads file and wraps it in an HTTP response            |


--------------

poll() is a system call that lets you monitor multiple file descriptors (sockets, pipes, etc.) to see if they are ready for reading, writing, or have an error.
It’s part of the POSIX standard and is available on all Unix-like systems

--------------

🧾 What is pollfd?
pollfd is a structure defined in <poll.h> used with the poll() system call.
It describes which file descriptor you want to monitor, what events you care about, and what actually happened.

🔧 Structure Definition:
cpp
Copy
Edit
struct pollfd {
    int   fd;       // File descriptor to monitor
    short events;   // Events to look for (input/output/error)
    short revents;  // Events that actually occurred (filled by poll())
};
🔹 Purpose of Each Field
Field	Description
fd	The file descriptor (e.g., socket or pipe) you want to monitor
events	What you're interested in (e.g. read, write, error)
revents	Set by poll() — tells you what actually happened to the fd

🔧 Common events Values (you set these)
Macro		Meaning
POLLIN		Data available to read
POLLOUT		Ready to write
POLLERR		Error occurred
POLLHUP		Hang-up (connection closed)