#!/bin/bash

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
NC='\033[0m'

# Helpers
divider() {
    echo -e "${CYAN}------------------------------------------------------------${NC}"
}

section() {
    divider
    echo -e "${BLUE}$1${NC}"
    divider
}

result_ok() {
    echo -e "${GREEN}[OK] $1${NC}"
}

result_fail() {
    echo -e "${RED}[FAIL] $1${NC}"
}

# Start
echo
section "Setup: Creating test files and directories"
mkdir -p www/upload
touch www/upload/1.txt
echo "This is a test file." > test.txt

# Tests
echo
section "Test 1: POST /echo (application/x-www-form-urlencoded)"
curl -s -w "\n${YELLOW}HTTP %{http_code}${NC}\n" \
     -X POST http://localhost:8080/echo \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "hello=world&foo=bar" | tee result_echo.txt

echo
section "Test 2: POST /upload (multipart/form-data)"
curl -s -w "\n${YELLOW}HTTP %{http_code}${NC}\n" \
     -X POST http://localhost:8080/upload \
     -F "file=@test.txt" | tee result_upload.txt

echo
section "Test 3: DELETE /delete/1.txt"
curl -s -w "\n${YELLOW}HTTP %{http_code}${NC}\n" \
     -X DELETE http://localhost:8080/upload/1.txt | tee result_delete.txt

echo
section "Test 4: GET /cgi-bin/test.py"
curl -s -i -w "\n${YELLOW}HTTP %{http_code}${NC}\n" http://localhost:8080/cgi-bin/test.py | tee result_cgi_get.txt


echo
section "Test 5: POST /cgi-bin/test.py with raw data"
curl -s -i -X POST http://localhost:8080/cgi-bin/test.py \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "name=test" | tee result_cgi_post.txt


# Result Checks
echo
section "Checking Results"

uploaded_file=$(ls www/upload/test.txt_* 2>/dev/null | head -n1)
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
echo
section "Cleanup"
rm -f test.txt result_*.txt

divider
echo -e "${YELLOW}==> All tests completed.${NC}"
divider
