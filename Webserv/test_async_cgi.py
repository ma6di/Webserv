#!/usr/bin/env python3

"""
Test script to verify async CGI functionality.
This script will take some time to execute to test the non-blocking nature.
"""

import sys
import time
import os

print("Content-Type: text/html\r")
print("\r")
print("<html><head><title>Async CGI Test</title></head><body>")
print("<h1>Async CGI Test Results</h1>")
print("<p>Script started at: {}</p>".format(time.strftime("%Y-%m-%d %H:%M:%S")))

# Simulate some processing time
print("<p>Processing for 2 seconds...</p>")
time.sleep(2)

print("<p>Script completed at: {}</p>".format(time.strftime("%Y-%m-%d %H:%M:%S")))
print("<p>This CGI script was executed asynchronously!</p>")
print("<ul>")
print("<li>Process ID: {}</li>".format(os.getpid()))
print("<li>Python version: {}</li>".format(sys.version))
if "QUERY_STRING" in os.environ:
    print("<li>Query string: {}</li>".format(os.environ["QUERY_STRING"]))
print("</ul>")
print("</body></html>")
