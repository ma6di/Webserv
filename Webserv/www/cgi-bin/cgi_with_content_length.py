#!/usr/bin/env python3
body = "<html><body>HelloWorldThisShouldBeIgnored</body></html>"
print("Content-Type: text/html")
print("Content-Length: 10")
print()
print(body)