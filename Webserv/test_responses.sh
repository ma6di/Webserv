#!/bin/bash

# Test script for response improvements
cd /Users/mahdicheraghali/Desktop/webserv/Webserv

# Start server in background
./webserv &
server_pid=$!
sleep 2

echo "Testing improved error responses..."

# Test 1: Missing HTTP version (should return 400)
echo "=== Test 1: Missing HTTP version ==="
printf "GET /missing_http_version\r\n\r\n" | nc -w 3 localhost 8080

echo -e "\n=== Test 2: Invalid HTTP version ==="
printf "GET / HTTP/0.9\r\nHost: localhost\r\n\r\n" | nc -w 3 localhost 8080

echo -e "\n=== Test 3: 404 Not Found ==="
curl -s -i http://localhost:8080/nonexistent

echo -e "\n=== Test 4: 411 Length Required ==="
curl -s -i -X POST http://localhost:8080/upload

# Stop server
kill $server_pid
wait $server_pid 2>/dev/null

echo -e "\nTests completed!"
