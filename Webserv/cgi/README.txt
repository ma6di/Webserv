# Module: CGI Executor (`CGIHandler`)

## Purpose

- Executes CGI scripts (e.g., `.py`, `.php`) when a matching location is configured.
- Passes HTTP request data via stdin and environment variables.
- Reads CGI output via stdout and returns it to the client.

## Components

- `CGIHandler.hpp` / `CGIHandler.cpp`

## Responsibilities

- Set up CGI environment variables (e.g., `REQUEST_METHOD`, `SCRIPT_NAME`, `QUERY_STRING`, etc.).
- Handle process creation and communication using `pipe()`, `fork()`, `dup2()`, and `execve()`.
- Send request body to the CGI script via stdin (for POST).
- Read CGI script output (headers + body) from stdout.
- Parse and validate CGI output, ensuring required headers (like `Content-Type`) are present.
- Handle timeouts and errors robustly.

## Integration

- Called by `WebServer` when a request matches a CGI-enabled location.
- Uses helpers to determine if a request should be handled as CGI and to build the correct environment.

## Output

- Returns a raw string containing the CGI script's response (headers + body), which is then parsed and sent as an HTTP response.

---

## Key Concepts

### CGI (Common Gateway Interface)

CGI is a standard way for a web server to run external scripts (like Python, PHP, or shell) and use their output as the HTTP response.

- The server passes request data to the script via:
  - **stdin**: For POST request body
  - **Environment variables**: Such as `REQUEST_METHOD`, `SCRIPT_NAME`, `QUERY_STRING`, etc.
- The script writes its response to **stdout**:
  - Starts with HTTP headers (e.g., `Content-Type: text/html`)
  - Followed by a blank line, then the response body

### Example CGI Flow

1. **Server** creates pipes for input/output.
2. **Server** forks a child process.
3. **Child** sets up environment and replaces itself with the CGI script using `execve()`.
4. **Parent** writes request body to the child's stdin (if needed) and reads the script's output from stdout.
5. **Server** parses the CGI output and sends it to the client.

### Example Environment Variables

| Variable         | Description                        |
|------------------|------------------------------------|
| REQUEST_METHOD   | HTTP method (GET, POST, etc.)      |
| SCRIPT_NAME      | Path to the CGI script             |
| QUERY_STRING     | URL query string                   |
| CONTENT_TYPE     | MIME type of POST data             |
| CONTENT_LENGTH   | Length of POST data                |
| PATH_INFO        | Extra path info after script name  |
| SERVER_PROTOCOL  | HTTP version                       |
| SERVER_SOFTWARE  | Server identification              |

---

## Troubleshooting

- If a CGI script fails, check stderr output and exit status.
- If the CGI output is missing required headers, a 500 error is returned.
- Timeouts are enforced to prevent hanging scripts.

---

Let us know if you want more details on CGI environment, debugging, or extending support for other
