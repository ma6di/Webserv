#!/usr/bin/env python3
import cgi
import os

UPLOAD_DIR = os.path.join(os.path.dirname(__file__), '..', 'upload')

print("Content-Type: text/html\r\n")

form = cgi.FieldStorage()
if "file" not in form:
    print("<html><body><h1>No file uploaded</h1></body></html>")
    exit()

fileitem = form["file"]
if not fileitem.filename:
    print("<html><body><h1>No file selected</h1></body></html>")
    exit()

filename = os.path.basename(fileitem.filename)
target_path = os.path.join(UPLOAD_DIR, filename)

try:
    with open(target_path, 'wb') as f:
        f.write(fileitem.file.read())
    print(f"<html><body><h1>File '{filename}' uploaded successfully!</h1></body></html>")
except Exception as e:
    print(f"<html><body><h1>Failed to upload file: {e}</h1></body></html>")