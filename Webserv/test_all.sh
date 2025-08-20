#!/bin/bash

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
NC='\033[0m'

LOGFILE="test_results.log"
: > "$LOGFILE" # Clear log file at start

divider() {
    echo -e "${CYAN}------------------------------------------------------------${NC}"
    echo "------------------------------------------------------------" >> "$LOGFILE"
}

section() {
    divider
    echo -e "${BLUE}$1${NC}"
    echo "== $1 ==" >> "$LOGFILE"
    divider
}

result_ok() {
    echo -e "${GREEN}[OK] $1${NC}"
    echo "[OK] $1" >> "$LOGFILE"
}

result_fail() {
    echo -e "${RED}[FAIL] $1${NC}"
    echo "[FAIL] $1" >> "$LOGFILE"
}

log_and_run() {
    # $1 = description, $2 = command, $3 = output file, $4 = grep pattern, $5 = expected string for OK
    section "$1"
    echo ">> $2" >> "$LOGFILE"
    eval "$2" > "$3" 2>&1
    cat "$3" >> "$LOGFILE"
    echo "" >> "$LOGFILE"

    if [ -n "$4" ]; then
        if grep -q "$4" "$3"; then
            result_ok "$5"
        else
            fail_line=$(grep -m1 -E "HTTP/|error|fail|not found|denied|forbidden|timeout" "$3")
            result_fail "$5"
            if [ -n "$fail_line" ]; then
                echo -e "${YELLOW}Reason: $fail_line${NC}"
            fi
        fi
    fi
}

# Start
section "Setup: Creating test files and directories"
mkdir -p www/upload
touch www/upload/1.txt
echo "This is a test file." > test.txt

# Tests
# log_and_run "Test 1: POST /echo (application/x-www-form-urlencoded)" \
#     "curl -s -i -w \"\nHTTP %{http_code}\n\" -X POST http://localhost:8080/echo -H \"Content-Type: application/x-www-form-urlencoded\" -d \"hello=world&foo=bar\"" \
#     result_echo.txt "HTTP/1.1 200" "POST /echo returned 200 OK"

log_and_run "Test 2: POST /upload (multipart/form-data)" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X POST http://localhost:8080/upload -F \"file=@test.txt\"" \
    result_upload.txt "HTTP/1.1 201" "POST /upload returned 201 created"

log_and_run "Test 3: DELETE /1.txt" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X DELETE http://localhost:8080/upload/1.txt" \
    result_delete.txt "HTTP/1.1 204" "DELETE /1.txt returned 204 204 No Content"

log_and_run "Test 4: GET /cgi-bin/test.py" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" http://localhost:8080/cgi-bin/test.py" \
    result_cgi_get.txt "HTTP/1.1 200" "CGI GET returned 200 OK"

log_and_run "Test 4b: CGI with Content-Length" \
    "curl -s -i http://localhost:8080/cgi-bin/cgi_with_content_length.py" \
    result_cgi_content_length.txt "<html><bod" "CGI with Content-Length returns only specified bytes."

log_and_run "Test 5: GET /cgi-bin/test.py with raw data" \
    "curl -s -i -X GET http://localhost:8080/cgi-bin/test.py -H \"Content-Type: application/x-www-form-urlencoded\" -d \"name=test\"" \
    result_cgi_post.txt "HTTP/1.1 200" "CGI GET with data returned 200 OK"

log_and_run "Test 6: POST /cgi-bin/cgi_post.py with file upload" \
    "curl -s -i -X POST http://localhost:8080/cgi-bin/cgi_post.py -F \"file=@test.cpp\"" \
    result_cgi_post_file.txt "HTTP/1.1 200" "CGI POST (file) returned 200 OK"

log_and_run "Test 7: GET /cgi-bin/cgi_path_info.py/foo/bar (PATH_INFO test)" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" http://localhost:8080/cgi-bin/cgi_path_info.py/foo/bar" \
    result_cgi_path_info.txt "/foo/bar" "CGI PATH_INFO correctly set and returned."

log_and_run "Test 8: POST /cgi-bin/echo_body.py with chunked encoding" \
    "curl -s -i -X POST http://localhost:8080/cgi-bin/echo_body.py -H \"Transfer-Encoding: chunked\" -d \"ChunkedBodyTest\"" \
    result_cgi_chunked.txt "ChunkedBodyTest" "CGI received and echoed chunked body correctly."

log_and_run "Test 9: 501 Not Implemented" \
    "curl -s -i -X PATCH http://localhost:8080/" \
    result_501.txt "501 Not Implemented" "501 Not Implemented error returned."

log_and_run "Test 10: 405 Method Not Allowed" \
    "curl -s -i -X DELETE http://localhost:8080/" \
    result_405.txt "405 Method Not Allowed" "405 Method Not Allowed error returned."

log_and_run "Test 11: 413 Payload Too Large" \
    "curl -s -X POST http://localhost:8080/upload -F "file=@bigfile.txt"" \
    result_413.txt "413 Payload Too Large" "413 Payload Too Large error returned."

# log_and_run "Test 12: 400 Bad Request" \
#     "printf \"GET /missing_http_version\r\n\r\n\" | nc localhost 8080" \
#     result_400.txt "400 Bad Request" "400 Bad Request error returned."

log_and_run "Test 13: 401 Unauthorized" \
    "curl -s -i http://localhost:8080/should_require_auth" \
    result_401.txt "401 Unauthorized" "401 Unauthorized error returned."

log_and_run "Test 14: 403 Forbidden" \
    "touch www/upload/forbidden.txt && chmod 444 www/upload/forbidden.txt && curl -s -i -F "file=@test.cpp" http://localhost:8080/upload/forbidden.txt" \
    result_403.txt "403 Forbidden" "403 Forbidden error returned."
chmod 644 www/upload/forbidden.txt
rm -f www/upload/forbidden.txt

log_and_run "Test 15: 404 Not Found" \
    "curl -s -i http://localhost:8080/doesnotexist.txt" \
    result_404.txt "404 Not Found" "404 Not Found error returned."



section "Test 16: 408 Request Timeout"
echo "Testing 408 Request Timeout by connecting but not sending data..." >> "$LOGFILE"
# Connect to server and wait for server to respond or close after timeout
nc localhost 8080 > result_408.txt 2>&1 &  # run in background
nc_pid=$!

# Wait slightly longer than server timeout (e.g., 12s) to ensure 408 triggers
sleep 12

# Kill nc if still running
kill $nc_pid 2>/dev/null

# Append result to log
cat result_408.txt >> "$LOGFILE"

# Check for 408 response
if grep -q "408 Request Timeout" result_408.txt; then
    result_ok "408 Request Timeout returned for idle connection."
else
    if [ -s result_408.txt ]; then
        fail_line=$(grep -m1 -E "HTTP/|error|fail|not found|denied|forbidden|timeout" result_408.txt)
        result_fail "408 Request Timeout missing for idle connection."
        if [ -n "$fail_line" ]; then
            echo -e "${YELLOW}Reason: $fail_line${NC}"
        fi
    else
        result_fail "408 Request Timeout test: Connection failed or no response received."
        echo -e "${YELLOW}Note: Make sure server is running and timeout is configured correctly.${NC}"
    fi
fi


log_and_run "Test 17: 500 Internal Server Error" \
    "echo -e '#!/usr/bin/env python3\nimport sys\nsys.exit(1)' > www/cgi-bin/error500.py && chmod +x www/cgi-bin/error500.py && curl -s -i http://localhost:8080/cgi-bin/error500.py" \
    result_500.txt "500 Internal Server Error" "500 Internal Server Error returned."
rm -f www/cgi-bin/error500.py

section "Test 18: 502 Bad Gateway (manual/visual)"
echo "Manual/visual: To test 502, configure your server as a proxy to a dead upstream." >> "$LOGFILE"
result_ok "502 Bad Gateway test: Please check server logs or proxy config."

section "Test 19: 503 Service Unavailable (manual/visual)"
echo "Manual/visual: To test 503, simulate server overload or maintenance mode." >> "$LOGFILE"
result_ok "503 Service Unavailable test: Please check server logs or maintenance mode."

section "Test 20: 504 Gateway Timeout (CGI timeout test)"
echo -e '#!/usr/bin/env python3\nimport time\ntime.sleep(100)' > www/cgi-bin/hang.py
chmod +x www/cgi-bin/hang.py
curl -s -i --max-time 10 http://localhost:8080/cgi-bin/hang.py > result_504.txt
cat result_504.txt >> "$LOGFILE"
if grep -q "504 Gateway Timeout" result_504.txt; then
    result_ok "504 Gateway Timeout error returned for CGI timeout."
else
    fail_line=$(grep -m1 -E "HTTP/|error|fail|not found|denied|forbidden|timeout" result_504.txt)
    result_fail "504 Gateway Timeout error missing for CGI timeout."
    if [ -n "$fail_line" ]; then
        echo -e "${YELLOW}Reason: $fail_line${NC}"
    fi
fi


# rm -f www/cgi-bin/hang.py

# Result Checks
section "Checking Results"

uploaded_file=$(ls www/upload/test_* 2>/dev/null | head -n1)
if [ -n "$uploaded_file" ] && grep -q "This is a test file." "$uploaded_file"; then
    result_ok "File uploaded successfully as $uploaded_file."
else
    uploaded_file=$(ls www/upload_* 2>/dev/null | head -n1)
    if [ -n "$uploaded_file" ]; then
        result_ok "Raw data uploaded as $uploaded_file."
    else
        result_fail "File upload failed or is empty."
    fi
fi

if [ ! -f www/upload/1.txt ]; then
    result_ok "File deleted successfully."
else
    result_fail "File was not deleted."
fi

if grep -qi "Content-Type:" result_cgi_get.txt; then
    result_ok "CGI GET returned Content-Type header."
else
    result_fail "CGI GET missing Content-Type header."
fi

if grep -qi "Content-Type:" result_cgi_post.txt; then
    result_ok "CGI POST returned Content-Type header."
else
    result_fail "CGI POST missing Content-Type header."
fi

# Cleanup
# section "Cleanup"
# rm -f test.txt result_*.txt

divider
echo -e "${YELLOW}==> All tests completed. See $LOGFILE for details.${NC}"
divider