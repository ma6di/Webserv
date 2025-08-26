#!/usr/bin/env python3
import os
import json

UPLOAD_DIR = "../upload"  # or "./www/images/photobook" if you want only those

def files_json(directory):
    try:
        return [
            f for f in os.listdir(directory)
            if os.path.isfile(os.path.join(directory, f))
        ]
    except Exception as e:
        return []

print("Content-Type: application/json\r\n")
print()
print(json.dumps(files_json(UPLOAD_DIR)))