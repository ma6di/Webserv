#!/usr/bin/env python3
import cgi
import os
import json
import time
from datetime import datetime
import sys

# Helper to print JSON with optional status
def send_response(obj, status_code=200):
    print(f"Status: {status_code}")
    print("Content-Type: application/json\n")
    print(json.dumps(obj))
    sys.exit()

upload_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../images/uploads"))
os.makedirs(upload_dir, exist_ok=True)

form = cgi.FieldStorage()
response = {}

ALLOWED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".gif", ".webp"}

if "file" in form and form["file"].filename:
    fileitem = form["file"]
    original_name = os.path.basename(fileitem.filename)
    _, ext = os.path.splitext(original_name)
    ext = ext.lower()

    if ext not in ALLOWED_EXTENSIONS:
        send_response({"error": f"File type '{ext}' not allowed."}, status_code=400)

    # 📝 Use user-provided name if given
    user_name = form.getvalue("name", "").strip()
    safe_name = user_name.replace(" ", "_") if user_name else "photo"

    # 🕒 Append timestamp with ms precision
    now = datetime.now()
    timestamp = now.strftime("%Y%m%d-%H%M%S") + f"-{int(now.microsecond / 1000):03d}"
    filename = f"{safe_name}_{timestamp}{ext}"

    filepath = os.path.join(upload_dir, filename)

    try:
        with open(filepath, "wb") as f:
            f.write(fileitem.file.read())
    except Exception as e:
        send_response({"error": f"Failed to save file: {str(e)}"}, status_code=500)

    send_response({"status": "success", "file": filename}, status_code=200)

else:
    send_response({"error": "Missing or invalid file"}, status_code=400)
