#!/usr/bin/env python3
import sys
import os
import urllib.parse

print("Content-Type: text/html")
print()

print("<html><head><title>Echo Response</title></head><body>")
print("<h1>Echo Response</h1>")

# Get request method
method = os.environ.get('REQUEST_METHOD', 'UNKNOWN')
print(f"<p><strong>Method:</strong> {method}</p>")

# Handle POST data
if method == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print(f"<p><strong>POST Data:</strong> {post_data}</p>")
        
        # Parse form data
        if post_data:
            parsed = urllib.parse.parse_qs(post_data)
            print("<h2>Parsed Form Data:</h2><ul>")
            for key, values in parsed.items():
                for value in values:
                    print(f"<li><strong>{key}:</strong> {value}</li>")
            print("</ul>")

# Handle GET parameters
query_string = os.environ.get('QUERY_STRING', '')
if query_string:
    print(f"<p><strong>Query String:</strong> {query_string}</p>")
    parsed = urllib.parse.parse_qs(query_string)
    print("<h2>Parsed Query Parameters:</h2><ul>")
    for key, values in parsed.items():
        for value in values:
            print(f"<li><strong>{key}:</strong> {value}</li>")
    print("</ul>")

print("</body></html>")
