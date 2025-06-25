
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

 ------------
 
🧠 Key Concepts
Term			Meaning
HTTP Request	What the client sends to the server
Method			HTTP verb like GET, POST, DELETE, etc.
Path			The URI being requested (e.g. /upload)
Headers			Key-value metadata (e.g. Content-Type: text/html)
Body			Optional data sent with the request (only for POST/PUT)

----------------

## 🌐 What is HTTP?

**HTTP (HyperText Transfer Protocol)** is the language browsers and servers use to communicate.

A basic HTTP request looks like this:

```
GET /index.html HTTP/1.1
Host: localhost:8080
User-Agent: curl/7.68.0
Accept: */*

[Optional body for POST/PUT]
```

---

## 🧱 Components of an HTTP Request

### 1. **Request Line**

This is the **first line** of the HTTP request.

```
GET /index.html HTTP/1.1
```

| Part        | Meaning                                                                |
| ----------- | ---------------------------------------------------------------------- |
| **Method**  | What action the client wants to perform (GET, POST, DELETE, etc.)      |
| **Path**    | The target resource on the server (`/`, `/index.html`, `/upload/file`) |
| **Version** | HTTP version used (`HTTP/1.0`, `HTTP/1.1`, `HTTP/2.0`)                 |

---

### 2. **Headers**

These are **key-value pairs** sent by the client to provide metadata about the request.

Example:

```
Host: localhost:8080
User-Agent: curl/7.68.0
Accept: text/html
```

| Header Name      | Meaning                                                                                                                    |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `Host`           | Specifies the domain (required in HTTP/1.1)                                                                                |
| `User-Agent`     | Browser or client info (e.g. curl, Chrome, Firefox)                                                                        |
| `Accept`         | What content types the client accepts (e.g. text/html, application/json)                                                   |
| `Content-Type`   | (for POST) What type of data is being sent (e.g. application/x-www-form-urlencoded, multipart/form-data, application/json) |
| `Content-Length` | (for POST) How many bytes in the body                                                                                      |
| `Connection`     | `keep-alive` or `close` for TCP connection handling                                                                        |

---

### 3. **Body** (Only for POST/PUT requests)

If the client sends data (e.g., form submission, file upload), it appears **after the blank line** separating headers from body.

Example:

```
POST /upload HTTP/1.1
Host: localhost:8080
Content-Length: 13
Content-Type: text/plain

Hello, server!
```

Body = `"Hello, server!"` (13 bytes)

---

## 🔁 What does your server do with each part?

| Part    | Parsed by               | Used for                                    |
| ------- | ----------------------- | ------------------------------------------- |
| Method  | `Request::getMethod()`  | To route the request (GET/POST logic)       |
| Path    | `Request::getPath()`    | To find the file or resource to serve       |
| Version | `Request::getVersion()` | For response formatting                     |
| Headers | `Request::getHeader()`  | To process `Content-Type`, `Host`, etc.     |
| Body    | `Request::getBody()`    | For POST forms, JSON, uploads, or CGI input |

---

## 🧠 Summary Table

| Term        | Example                   | Description                                         |
| ----------- | ------------------------- | --------------------------------------------------- |
| **Method**  | `GET`, `POST`             | HTTP action verb (what to do)                       |
| **Path**    | `/index.html`, `/upload`  | What resource is being requested                    |
| **Version** | `HTTP/1.1`                | Protocol version                                    |
| **Header**  | `Content-Type: text/html` | Extra info about the request                        |
| **Body**    | `{"name":"John"}`         | Data sent with the request (only for POST/PUT/etc.) |

---

### 🔍 Example Breakdown

```http
POST /cgi-bin/script.py HTTP/1.1
Host: localhost:8080
Content-Type: application/x-www-form-urlencoded
Content-Length: 27

username=mahdi&password=1234
```

| Element               | Value                               |
| --------------------- | ----------------------------------- |
| Method                | `POST`                              |
| Path                  | `/cgi-bin/script.py`                |
| Version               | `HTTP/1.1`                          |
| Header\[Host]         | `localhost:8080`                    |
| Header\[Content-Type] | `application/x-www-form-urlencoded` |
| Body                  | `username=mahdi&password=1234`      |

---

Let me know if you’d like to see an example from **curl**, **telnet**, or how to handle each of these in your own code (`Request`, `Router`, etc.).
