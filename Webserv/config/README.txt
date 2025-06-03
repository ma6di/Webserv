
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
