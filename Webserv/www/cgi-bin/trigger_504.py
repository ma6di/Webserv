#!/usr/bin/env python3
import time
time.sleep(100)  # sleep longer than your gateway timeout (e.g. 10s)
print("Content-Type: text/plain\n")
print("This should never return before timeout.")
