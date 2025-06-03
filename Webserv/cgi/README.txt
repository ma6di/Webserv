
**Module:** CGI Executor (`CGIHandler`)

**Purpose:**

* Executes CGI scripts (e.g., `.py`, `.php`) when a matching location is configured.
* Passes HTTP request data via stdin and environment variables.
* Reads CGI output via stdout.

**Components:**

* `CGIHandler.hpp` / `CGIHandler.cpp`

**Responsibilities:**

* Set up environment (REQUEST\_METHOD, SCRIPT\_NAME, etc.).
* Handle stdin/stdout using `pipe()`, `fork()`, `dup2()`, `execve()`.
* Read output and return it as raw HTTP string.

**Integration:**

* Called by `WebServer::handle_client_data()` when a CGI path is matched.
* Uses helpers from `utils/RequestUtils.cpp` to determine if a path is CGI.

**Output:**

* Raw string containing CGI script response (headers + body).
