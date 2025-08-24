#!/usr/bin/env python3
import sys

print("Content-Type: text/plain")
print()

# Read and echo the body
body = sys.stdin.read()
print(body)