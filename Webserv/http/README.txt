
**Module:** HTTP Request/Response

**Purpose:**

* `Request`: Parses incoming HTTP requests.
* `Response`: Builds outgoing HTTP responses (TBD).

**Components:**

* `Request.hpp` / `Request.cpp`
* (Planned: `Response.hpp` / `Response.cpp`)

**Request Responsibilities:**

* Extract `method`, `path`, `version`, headers, body from raw HTTP string.
* Provide accessors for server logic.

**Planned Response Responsibilities:**

* Set status code, headers, body.
* Format as complete HTTP response string.

**Integration:**

* `Request` used inside `WebServer::handle_client_data()`.
* `Response` will be used to send CGI/static responses.
