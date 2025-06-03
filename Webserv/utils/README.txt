
**Module:** Utilities and Helpers

**Purpose:**

* Provide reusable logic for routing, path resolution, and CGI detection.

**Components:**

* `RequestUtils.hpp` / `RequestUtils.cpp`

**Responsibilities:**

* `match_location()` — matches a URI to best `LocationConfig`
* `is_cgi_request()` — checks URI extension against config
* `resolve_script_path()` — builds filesystem path to CGI script

**Integration:**

* Used by `WebServer` to determine how to route a request
* Pure functions; no side effects

---

Let me know if you’d like to include test plans or future feature documentation!
