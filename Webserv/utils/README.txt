# Utilities and Helpers (`utils/`)

## Purpose

Provide reusable logic for:
- Location and path resolution
- CGI detection
- File and directory utilities
- HTTP body decoding

## Components

- `utils.hpp` / `utils.cpp`

## Responsibilities

- `file_exists(path)` — Check if a file exists on disk.
- `get_mime_type(path)` — Guess MIME type from file extension.
- `match_location(locations, path)` — Find the best-matching `LocationConfig` for a given URI.
- `is_cgi_request(loc, uri)` — Determine if a request should be handled as CGI based on location and URI.
- `decode_chunked_body(body)` — Decode HTTP chunked transfer encoding.
- `is_directory(path)` — Check if a path is a directory.
- `generate_directory_listing(dir_path, uri_path)` — Generate an HTML directory listing for autoindex.

## Integration

- Used by `WebServer` and handlers for routing, CGI, static file serving, and directory listing.
- All functions are stateless and pure (no side effects except logging).

---

**See also:**  
- `LocationConfig.hpp` for location structure  
- `Config.hpp` for server configuration

Let us know if you need test plans or want to document future utility
