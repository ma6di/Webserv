#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/html")
print()
print("<html><body><h1>Hello from CGI Python!</h1></body></html>")

print("CGI script ran!", file=sys.stderr)
print("PATH=" + os.environ.get("PATH", ""), file=sys.stderr)
print("PYTHONPATH=" + os.environ.get("PYTHONPATH", ""), file=sys.stderr)
print("SCRIPT_FILENAME=" + os.environ.get("SCRIPT_FILENAME", ""), file=sys.stderr)
