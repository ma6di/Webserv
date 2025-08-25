#!/usr/bin/env python3
import os
import json

UPLOAD_DIR = "../upload"  # or "./www/images/photobook" if you want only those
IMAGE_EXTENSIONS = {'.jpg', '.jpeg', '.png', '.gif'}

def photobook_json(directory):
    try:
        return [
            f for f in os.listdir(directory)
            if os.path.isfile(os.path.join(directory, f)) and os.path.splitext(f)[1].lower() in IMAGE_EXTENSIONS
        ]
    except Exception as e:
        return []

print("Content-Type: application/json\r\n")
print()
print(json.dumps(photobook_json(UPLOAD_DIR)))

"""
JESS: this script returns a json file that lists all the file names in images/photobooth. Needed from the frontend.
"""

