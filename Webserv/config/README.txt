
**Module:** Configuration Parser (`Config`)

**Purpose:**

* Parses the `default.conf` file.
* Stores global settings (e.g., port, root) and location blocks.
* Provides accessors to retrieve configuration data for other modules.

**Components:**

* `Config.hpp` / `Config.cpp`
* `LocationConfig` struct

**Responsibilities:**

* Parse lines for `listen`, `root`, `error_page`, `location {}` blocks.
* Strip semicolons, validate syntax.
* Match URIs to longest-prefix location blocks.

**Integration:**

* Used globally (via `g_config`) or injected into `WebServer`.
* Used by `WebServer`, `RequestRouter`, and `CGIHandler`.

**Output:**

* Parsed values stored in:

  * `int port`
  * `std::string root`
  * `std::vector<LocationConfig>`
  * `std::map<int, std::string> error_pages`

--------------------------------
Key Concepts:
In web server configuration (like in NGINX, which WebServ mimics), 
a location block defines how to handle requests for a specific URI path.

For example:

	location / {
		root /www/html;
		index index.html;
	}

	location /cgi-bin {
		root /www/cgi;
		cgi_extension .py;
	}

The first block handles requests to /, /about, /index.html, etc.
The second block handles requests to /cgi-bin/script.py, which are CGI scripts.

Each location can:
	Define a different file root
	Set allowed HTTP methods (GET, POST, etc.)
	Enable CGI
	Turn on directory listing (autoindex)
	Set upload folder
