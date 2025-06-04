
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

------------------
Key Concept: 

CGI (Common Gateway Interface)
CGI is a standard way for a web server to run external scripts (like Python, PHP, or shell) and use the output as the HTTP response.

It’s like:
	“Here’s the request — run this script — give me the result — I’ll send it to the client.”
A CGI script receives input via:
	stdin: for POST request body
	environment variables: like REQUEST_METHOD, SCRIPT_NAME, etc.

It writes output to:
	stdout: typically a string starting with headers like Content-Type: ..., then a blank line, then body

2. HTTP Body, Header, Path, and Env

Term	Meaning
Body	The raw content of a POST request (e.g., form data or uploaded file)
Header	Extra info like Content-Type, Host, Content-Length, etc.
Path	The URL the user requested (e.g., /cgi-bin/script.py)
Env		Environment variables passed to the CGI script (REQUEST_METHOD=POST)

-----------------------

Fork + Pipe
To run a CGI script:
	You create two pipes (for input and output).
	You fork()
	The child process replaces itself with the script (execve)
	The parent sends input and reads output from the pipes

-------------
