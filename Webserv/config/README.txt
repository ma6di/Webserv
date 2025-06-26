# Module: Configuration Parser (`Config`)

## Purpose

- Parses the `default.conf` configuration file.
- Stores global server settings (e.g., ports, root directory) and location blocks.
- Provides accessor methods for other modules to retrieve configuration data.

## Components

- `Config.hpp` / `Config.cpp` — Implements the `Config` class for parsing and storing server configuration.
- `LocationConfig.hpp` — Defines the `LocationConfig` struct for location-specific settings.

## Responsibilities

- Parse configuration lines for:
  - `listen` (port numbers)
  - `root` (global or per-location root directory)
  - `error_page` (custom error pages)
  - `location { ... }` blocks (location-specific settings)
  - `client_max_body_size`
- Strip semicolons and validate syntax.
- Match URIs to the longest-prefix location block for request routing.

## Integration

- Used globally (via `g_config`) or injected into `WebServer`.
- Accessed by `WebServer`, request handlers, and `CGIHandler` for routing and settings.

## Output

- Parsed values are stored in:
  - `std::vector<int> ports` — All listen ports.
  - `std::string root` — Global root directory.
  - `std::vector<LocationConfig>` — All location blocks.
  - `std::map<int, std::string> error_pages` — Custom error pages.
  - `size_t max_body_size` — Maximum allowed request body size.

---

## Key Concepts

In web server configuration (similar to NGINX), a **location block** defines how to handle requests for a specific URI path.

**Example:**
```nginx
location / {
    root /www/html;
    index index.html;
}

location /cgi-bin {
    root /www/cgi;
    cgi_extension .py;
}
```
