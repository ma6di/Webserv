#!/usr/bin/env python3
import socket
import time
HOST = '127.0.0.1'  # your server's IP or localhost
PORT = 8080         # your server's port
# Create a socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
# Construct chunked HTTP request
request_headers = (
     "POST /cgi-bin/chunks.py HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
)
# Send headers
s.sendall(request_headers.encode())
# Send body in chunks
# Format: <chunk-size-in-hex>\r\n<data>\r\n
chunks = [
#     "4\r\nWiki\r\n",
#     "5\r\npedia\r\n",
#     "E\r\n in\r\n\r\nchunks.\r\n",
#     "0\r\n\r\n"  # last chunk
	"3\r\n"
	"abc\r\n"
	"2\r\n"
	"de\r\n"
	"0\r\n"
	"\r\n"
#    "3\nabc\r\n" # --> bad request
    # "5\r\n"
	# "abc\r\n"
]
for chunk in chunks:
    s.sendall(chunk.encode())
    time.sleep(0.5)  # simulate slow client
# Read response
response = s.recv(4096)
print("Server response:")
print(response.decode())
s.close()











