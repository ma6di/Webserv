
**Module:** Core HTTP Server

**Purpose:**

* Set up server socket.
* Accept client connections.
* Handle read/write with `poll()` loop.

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
