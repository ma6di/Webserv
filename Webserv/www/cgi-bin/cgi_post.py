#!/usr/bin/env python3
import cgi
import os

print("Content-Type: text/html\n")

upload_dir = os.path.join(os.path.dirname(__file__), "../upload")
os.makedirs(upload_dir, exist_ok=True)

form = cgi.FieldStorage()
if "file" in form:
    fileitem = form["file"]
    if fileitem.filename:
        fn = os.path.basename(fileitem.filename)
        upload_path = os.path.abspath(os.path.join(upload_dir, fn))
        with open(upload_path, "wb") as f:
            f.write(fileitem.file.read())
        print(f"<h1>File '{fn}' uploaded successfully to {upload_path}!</h1>")
    else:
        print("<h1>No file was uploaded</h1>")
else:
    print("<h1>No file field in form</h1>")