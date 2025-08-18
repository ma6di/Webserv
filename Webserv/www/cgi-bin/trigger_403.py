#!/usr/bin/env python3
import os, sys

# Locate the web root relative to this script: www/cgi-bin/ -> www/
WEB_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
target_dir = os.path.join(WEB_ROOT, "images", "uploads")
target = os.path.join(target_dir, "forbidden.txt")

try:
    # Ensure the directory exists and is writable by the server user
    os.makedirs(target_dir, exist_ok=True)

    # Create/write the file
    with open(target, "w") as f:
        f.write("You should not read this.\n")

    # Remove all perms so your server can't serve it → 403 on GET
    os.chmod(target, 0o000)

    # Minimal CGI response
    sys.stdout.write("Content-Type: text/html\r\n\r\n")
    sys.stdout.write("<h1>Forbidden file created</h1>")
    sys.stdout.write("<p>Now try accessing "
                     "<a href='/images/uploads/forbidden.txt'>forbidden.txt</a></p>")

except Exception as e:
    # Return a CGI-compliant error (don’t crash the server)
    sys.stdout.write("Status: 500 Internal Server Error\r\n")
    sys.stdout.write("Content-Type: text/plain\r\n\r\n")
    sys.stdout.write(f"CGI failed: {e}\n")
    # Optionally log traceback to stderr so your server logs capture it
    import traceback; traceback.print_exc(file=sys.stderr)
