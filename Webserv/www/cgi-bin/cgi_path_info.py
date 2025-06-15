#!/usr/bin/env python3
import os
print("Content-Type: text/html\n")
print("<html><body>")
print("<h1>PATH_INFO Test</h1>")
print("<p>PATH_INFO: {}</p>".format(os.environ.get("PATH_INFO", "")))
print("</body></html>")