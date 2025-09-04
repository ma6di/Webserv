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
echo "/*#include "WebServer.hpp"
#include "config/Config.hpp"
#include "cgi/CGIHandler.hpp"
#include "http/Request.hpp"
#include <iostream>
#include <map>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
*/" > test.cpp


#!/bin/zsh

# Tests
log_and_run "Test 1: POST /cgi-bin/echo.py (application/x-www-form-urlencoded)" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X POST http://localhost:8080/cgi-bin/echo.py -H \"Content-Type: application/x-www-form-urlencoded\" -d \"hello=world&foo=bar\"" \
    result_echo.txt "HTTP/1.1 200" "POST /cgi-bin/echo.py returned 200 OK"

log_and_run "Test 2: POST /upload (multipart/form-data)" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X POST http://localhost:8080/upload -F \"file=@test.txt\"" \
    result_upload.txt "HTTP/1.1 201" "POST /upload returned 201 created"

log_and_run "Test 2b: POST /upload (multipart/form-data) in second server" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X POST http://localhost:8085/new_files -F \"file=@test.txt\"" \
    result_upload_S2.txt "HTTP/1.1 201" "POST /upload returned 201 created"

log_and_run "Test 3: DELETE /1.txt" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" -X DELETE http://localhost:8080/upload/1.txt" \
    result_delete.txt "HTTP/1.1 204" "DELETE /1.txt returned 204 204 No Content"

log_and_run "Test 4: GET /cgi-bin/test.py" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" http://localhost:8080/cgi-bin/test.py" \
    result_cgi_get.txt "HTTP/1.1 200" "CGI GET returned 200 OK"

log_and_run "Test 5: CGI with Content-Length" \
    "curl -s -i http://localhost:8080/cgi-bin/cgi_with_content_length.py" \
    result_cgi_content_length.txt "<html><bod" "CGI with Content-Length returns only specified bytes."

log_and_run "Test 6: GET /cgi-bin/test.py with raw data" \
    "curl -s -i -X GET http://localhost:8080/cgi-bin/test.py -H \"Content-Type: application/x-www-form-urlencoded\" -d \"name=test\"" \
    result_cgi_post.txt "HTTP/1.1 200" "CGI GET with data returned 200 OK"

log_and_run "Test 7: POST /cgi-bin/cgi_post.py with file upload" \
    "curl -s -i -X POST http://localhost:8080/cgi-bin/cgi_post.py -F \"file=@test.cpp\"" \
    result_cgi_post_file.txt "HTTP/1.1 200" "CGI POST (file) returned 200 OK"

log_and_run "Test 8: GET /cgi-bin/cgi_path_info.py/foo/bar (PATH_INFO test)" \
    "curl -s -i -w \"\nHTTP %{http_code}\n\" http://localhost:8080/cgi-bin/cgi_path_info.py/foo/bar" \
    result_cgi_path_info.txt "/foo/bar" "CGI PATH_INFO correctly set and returned."

log_and_run "Test 9: POST /cgi-bin/echo_body.py with chunked encoding" \
    "curl -s -i -X POST http://localhost:8080/cgi-bin/echo_body.py -H \"Transfer-Encoding: chunked\" -d \"ChunkedBodyTest\"" \
    result_cgi_chunked.txt "ChunkedBodyTest" "CGI received and echoed chunked body correctly."

log_and_run "Test 10: 501 Not Implemented" \
    "curl -s -i -X PATCH http://localhost:8080/" \
    result_501.txt "501 Not Implemented" "501 Not Implemented error returned."

log_and_run "Test 11: 405 Method Not Allowed" \
    "curl -s -i -X DELETE http://localhost:8080/" \
    result_405.txt "405 Method Not Allowed" "405 Method Not Allowed error returned."

log_and_run "Test 12: 413 Payload Too Large" \
    "truncate -s 110M bigfile.txt && curl -s -X POST http://localhost:8080/upload -F \"file=@bigfile.txt\"" \
    result_413.txt "413 Payload Too Large" "413 Payload Too Large error returned."
# log_and_run "Test 12: 413 Payload Too Large" \
#     "curl -s -X POST http://localhost:8080/upload -F "file=@bigfile.txt"" \
#     result_413.txt "413 Payload Too Large" "413 Payload Too Large error returned."

log_and_run "Test 13: 400 Bad Request" \
    "printf \"GET /missing_http_version\r\n\r\n\" | nc localhost 8080" \
    result_400.txt "400 Bad Request" "400 Bad Request error returned."

log_and_run "Test 15: 403 Forbidden" \
    "touch www/upload/forbidden.txt && chmod 444 www/upload/forbidden.txt && curl -s -i -F "file=@test.cpp" http://localhost:8080/upload/forbidden.txt" \
    result_403.txt "403 Forbidden" "403 Forbidden error returned."
chmod 644 www/upload/forbidden.txt
rm -f www/upload/forbidden.txt

log_and_run "Test 16: 404 Not Found" \
    "curl -s -i http://localhost:8080/doesnotexist.txt" \
    result_404.txt "404 Not Found" "404 Not Found error returned."


log_and_run "Test 17: 500 Internal Server Error" \
    "curl http://localhost:8080/cgi-bin/error500.py" \
    result_500.txt "500 Internal Server Error" "500 Internal Server Error returned."

log_and_run "Test 18: 502 Bad Gateway" \
    "curl http://localhost:8080/cgi-bin/test502.py" \
    result_502.txt "502 Bad Gateway" "502 (Bad Gateway)"


log_and_run "Test 19: POST without Content_length" \
    "curl -s -i -X POST http://localhost:8080/upload -H  --data "1236565465446"" \
    result_411.txt "411 Length Required" "POST/ 411 Length Required"


log_and_run "Test 20: double Content_length" \
    "curl -i -s -H 'Content-Length: 3' -H 'Content-Length: 5' -d 'foo' http://localhost:8080/" \
    result_double_Content_length.txt "400 Bad Request" "400 Bad Request"

log_and_run "Test 21: Wrong Location" \
    "curl -s -i "http://localhost:8080/%00test"/" \
    result_Wrong_Location.txt "404 Not Found" "404 Not Found"

log_and_run "Test 22: Unsupported HTTP Version (HTTP/0.9)" \
    "printf 'GET /upload HTTP/0.9\r\nHost: localhost:8080\r\n\r\n' | nc localhost 8080" \
    result_Wrong_HTTP_Version.txt "505 HTTP Version Not Supported" "505 HTTP Version Not Supported"

log_and_run "Test 23: Malformed HTTP Version" \
    "printf 'GET /upload HTTP/ABC\r\nHost: localhost:8080\r\n\r\n' | nc localhost 8080" \
    result_Malformed_HTTP_Version.txt "400 Bad Request" "400 Bad Request for malformed version"

log_and_run "Test 24: Missing Colon in Content-Length Header" \
    "printf 'POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length 5\r\n\r\nhello' | nc localhost 8080" \
    result_Missing_Colon_Content_Length.txt "400 Bad Request" "400 Bad Request"

# Transfer-Encoding: chunked edge cases
section "Transfer-Encoding: chunked Edge Cases"

# Test: CONTENT-LENGTH in all caps
log_and_run "Test 25: CONTENT-LENGTH all caps" \
    "curl -s -i -X POST -H 'CONTENT_LENGTH: 5' --data 'abcde' http://localhost:8080/cgi-bin/echo_body.py" \
    result_content_length_caps.txt "HTTP/1.1 200" "CONTENT-LENGTH (all caps) header accepted and parsed"

log_and_run "Test 26: Valid chunked encoding (curl automatic)" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary 'hello world' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_valid.txt "hello world" "Valid chunked encoding processed correctly"

log_and_run "Test 27: Large chunked data" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary \"\$(python3 -c 'print(\"A\" * 1000)')\" http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_large.txt "AAA" "Large chunked data processed correctly"

log_and_run "Test 28: Empty chunked body" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary '' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_empty.txt "HTTP/1.1 200" "Empty chunked body handled correctly"

log_and_run "Test 29: Multiple small chunks (automatic)" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary 'chunk1chunk2chunk3' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_multiple.txt "chunk1chunk2chunk3" "Multiple chunks processed correctly"

log_and_run "Test 30: Chunked with special characters" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary 'Hello\nWorld\r\nTest!' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_special.txt "Hello" "Chunked data with special characters handled correctly"

log_and_run "Test 31: Chunked with binary data" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary \$'\\x00\\x01\\x02\\x03\\xFF' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_binary.txt "HTTP/1.1 200" "Chunked binary data handled correctly"

# Test chunked with Content-Length (should ignore Content-Length)
log_and_run "Test 32: Chunked with Content-Length header (should ignore CL)" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' -H 'Content-Length: 999' --data-binary 'test data' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_with_cl.txt "400 Bad Request" "Content-Length ignored when chunked encoding used"

# Test case sensitivity
log_and_run "Test 33: Transfer-Encoding case insensitive" \
    "curl -s -i -X POST -H 'TRANSFER-ENCODING: CHUNKED' --data-binary 'case test' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_case.txt "HTTP/1.1 200" "Transfer-Encoding is case insensitive"

# Invalid Transfer-Encoding tests
section "Invalid Transfer-Encoding Edge Cases"

log_and_run "Test 34: Unsupported Transfer-Encoding (gzip)" \
    "curl -s -i -X POST -H 'Transfer-Encoding: gzip' --data-binary 'test data' http://localhost:8080/cgi-bin/echo_body.py" \
    result_te_gzip.txt "501 Not Implemented" "Unsupported Transfer-Encoding handled (might accept or reject)"

log_and_run "Test 35: Invalid Transfer-Encoding value" \
    "curl -s -i -X POST -H 'Transfer-Encoding: invalid-encoding' --data-binary 'test data' http://localhost:8080/cgi-bin/echo_body.py" \
    result_te_invalid.txt "501 Not Implemented" "Invalid Transfer-Encoding value handled"

log_and_run "Test 36: Multiple Transfer-Encoding headers" \
    "curl -s -i -X POST -H 'Transfer-Encoding: gzip, chunked' --data-binary 'test data' http://localhost:8080/cgi-bin/echo_body.py" \
    result_te_multiple.txt "501 Not Implemented" "Multiple Transfer-Encoding headers handled"

log_and_run "Test 37: Transfer-Encoding with both chunked and Content-Length" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n5\r\nhello\r\n0\r\n\r\n' | nc -w 3 localhost 8080" \
    result_te_both_headers.txt "400 Bad Request" "Both Transfer-Encoding and Content-Length headers handled"

log_and_run "Test 38: Malformed chunked claim with plain body" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\nhello world without chunks\r\n\r\n' | nc -w 3 localhost 8080" \
    result_te_fake_chunked.txt "400 Bad Request" "Claiming chunked but sending plain body"

# Expect: 100-continue tests
section "Expect Header Tests"

log_and_run "Test 39: Expect 100-continue with POST" \
    "curl -s -i -X POST -H 'Expect: 100-continue' --data-binary 'test data for expect continue' http://localhost:8080/cgi-bin/echo_body.py" \
    result_expect_continue.txt "100 Continue" "Expect: 100-continue handled correctly"

log_and_run "Test 40: Large POST with Expect 100-continue" \
    "curl -s -i -X POST -H 'Expect: 100-continue' --data-binary \"\$(python3 -c 'print(\"ExpectData\" * 100)')\" http://localhost:8080/cgi-bin/echo_body.py" \
    result_expect_large.txt "100 Continue" "Large POST with Expect: 100-continue handled correctly"

# # Manual tests for edge cases that curl can't generate
section "Manual chunked encoding edge cases"

log_and_run "Test 41: Invalid chunk size (non-hex)" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\\r\\nHost: localhost:8080\\r\\nTransfer-Encoding: chunked\\r\\n\\r\\nGG\\r\\nhello\\r\\n0\\r\\n\\r\\n' | nc -w 3 localhost 8080" \
    result_chunked_invalid_size.txt "400 Bad Request" "Invalid chunk size returns 400 Bad Request"

log_and_run "Test 42: Missing final chunk (0)" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\\r\\nHost: localhost:8080\\r\\nTransfer-Encoding: chunked\\r\\n\\r\\n5\\r\\nhello\\r\\n' | nc -w 3 localhost 8080" \
    result_chunked_no_final.txt "400 Bad Request" "Missing final chunk returns 400 Bad Request"

log_and_run "Test 43: Chunk size larger than actual data" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\\r\\nHost: localhost:8080\\r\\nTransfer-Encoding: chunked\\r\\n\\r\\nA\\r\\nhello\\r\\n0\\r\\n\\r\\n' | nc -w 3 localhost 8080" \
    result_chunked_size_mismatch.txt "400 Bad Request" "Chunk size mismatch returns 400 Bad Request"

log_and_run "Test 44: Invalid Transfer-Encoding (not chunked)" \
    "curl -s -i -X POST -H 'Transfer-Encoding: gzip' --data-binary 'test' http://localhost:8080/cgi-bin/echo_body.py" \
    result_te_not_chunked.txt "501 Not Implemented" "Non-chunked Transfer-Encoding returns 501"

log_and_run "Test 45: Very long chunk size line" \
    "printf 'POST /cgi-bin/echo_body.py HTTP/1.1\\r\\nHost: localhost:8080\\r\\nTransfer-Encoding: chunked\\r\\n\\r\\n%s\\r\\nhello\\r\\n0\\r\\n\\r\\n' \"$(printf '%*s' 500 '' | tr ' ' '5')\" | nc -w 3 localhost 8080" \
    result_chunked_long_size.txt "400 Bad Request" "Very long chunk size line returns 400 Bad Request"

# Additional realistic tests
log_and_run "Test 46: Chunked POST to upload endpoint" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' --data-binary 'file content here' http://localhost:8080/upload" \
    result_chunked_upload.txt "HTTP/1.1 201 Created" "Chunked POST to upload endpoint returned 201 Created"

log_and_run "Test 47: Chunked with Connection: close" \
    "curl -s -i -X POST -H 'Transfer-Encoding: chunked' -H 'Connection: close' --data-binary 'connection test' http://localhost:8080/cgi-bin/echo_body.py" \
    result_chunked_conn_close.txt "connection test" "Chunked with Connection: close handled correctly"

# Test 48: Expect 100-continue with oversized Content-Length (should get 413)
log_and_run "Test 48: Expect 100-continue with oversized Content-Length (should get 413)" \
    "curl -i -s -X POST http://localhost:8080/ -H 'Expect: 100-continue' -H 'Content-Length: 100001' --data-binary @<(head -c 100001 /dev/zero)" \
    result_expect_413.txt "413 Payload Too Large" "Expect: 100-continue oversized CL triggers 413"

# Test 49: POST with Expect: 100-continue and no Content-Length or chunked encoding (should get 411)
log_and_run "Test 49: POST with Expect: 100-continue and no Content-Length or chunked encoding (should get 411)" \
    "curl -i -s -X POST http://127.0.0.1:8080/upload -H 'Expect: 100-continue' --data-binary \"\$(head -c 10240 /dev/zero)\"" \
    result_expect_411.txt "411 Length Required" "Expect: 100-continue without CL/chunked triggers 411"

log_and_run "Test 50: 301 redirect" \
    "curl localhost:8080/old" \
    result_redirect_301.txt "301 Redirect" "Redirect"

log_and_run "Test 51: 301 redirect" \
    "curl -i localhost:8080/upload" \
    result_dir_listing.txt "200 OK" "Directory Listing"

log_and_run "Test 52: 200 File Request" \
    "curl -i -X GET http://localhost:8080/upload/test.cpp" \
    result_file_request.txt "200 OK" "File Request"

section "Test 53: 408 Request Timeout"
# echo "Testing 408 Request Timeout by connecting but not sending data..." >> "$LOGFILE"
# Connect to server and wait for server to respond or close after timeout
nc -w 15 localhost 8080 > result_408.txt 
# nc_pid=$!
sleep 12
# kill $nc_pid 2>/dev/null
# sleep 1  # Give time for output to flush
# cat result_408.txt >> "$LOGFILE"

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

section "Test 54: 504 Gateway Timeout (CGI timeout test)"
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

# Result Checks
section "Checking Results"

uploaded_file=$(ls www/upload/test_* 2>/dev/null | head -n1)
if [ -n "$uploaded_file" ] && grep -q "This is a test file." "$uploaded_file"; then
    result_ok "File uploaded successfully as $uploaded_file."
else
    uploaded_file=$(ls www/upload/upload_* 2>/dev/null | head -n1)
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
section "Cleanup"
rm -f test.txt result_*.txt test.cpp bigfile.txt

divider
echo -e "${YELLOW}==> All tests completed. See $LOGFILE for details.${NC}"
divider