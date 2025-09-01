import os

# Ensure test file exists before running tests
def setup_test_file():
    upload_dir = os.path.join(os.path.dirname(__file__), 'www', 'upload')
    os.makedirs(upload_dir, exist_ok=True)
    test_file = os.path.join(upload_dir, '1.txt')
    with open(test_file, 'w') as f:
        f.write('This is a test file.')
    return test_file

# Remove test file after delete test
def cleanup_test_file():
    upload_dir = os.path.join(os.path.dirname(__file__), 'www', 'upload')
    test_file = os.path.join(upload_dir, '1.txt')
    if os.path.exists(test_file):
        os.remove(test_file)

import http.client
import socket
import ssl
import sys

SERVER = "127.0.0.1"
PORT = 8080

def print_result(name, passed, detail=""):
    GREEN = '\033[0;32m'
    RED = '\033[0;31m'
    NC = '\033[0m'
    status = f"{GREEN}PASS{NC}" if passed else f"{RED}FAIL{NC}"
    print(f"{name}: {status}")
    if detail:
        print(detail)

def request(method, path, headers=None, body=None, use_chunked=False, expect_continue=False):
    conn = http.client.HTTPConnection(SERVER, PORT, timeout=5)
    if expect_continue:
        if headers is None:
            headers = {}
        headers["Expect"] = "100-continue"
    if use_chunked:
        if headers is None:
            headers = {}
        headers["Transfer-Encoding"] = "chunked"
        conn.putrequest(method, path)
        for k, v in headers.items():
            conn.putheader(k, v)
        conn.endheaders()
        # Send chunked body
        if body:
            chunk = f"{len(body):X}\r\n{body}\r\n0\r\n\r\n"
            conn.send(chunk.encode())
    else:
        conn.request(method, path, body=body, headers=headers or {})
    try:
        resp = conn.getresponse()
        data = resp.read()
        return resp.status, resp.reason, data.decode(errors="replace"), resp.getheaders()
    except Exception as e:
        return None, None, str(e), []
    finally:
        conn.close()

def test_simple_get():
    status, reason, body, headers = request("GET", "/")
    print_result("Simple GET /", status == 200, f"Status: {status}, Reason: {reason}")

def test_not_found():
    status, reason, body, headers = request("GET", "/doesnotexist.txt")
    print_result("404 Not Found", status == 404, f"Status: {status}, Reason: {reason}")

def test_post_upload():
    status, reason, body, headers = request("POST", "/upload", headers={"Content-Type": "text/plain", "Content-Length": "15"}, body="Hello, webserv!\n")
    print_result("POST /upload", status in (200, 201), f"Status: {status}, Reason: {reason}")

def test_delete():
    status, reason, body, headers = request("DELETE", "/upload/1.txt")
    print_result("DELETE /upload/1.txt", status == 204, f"Status: {status}, Reason: {reason}")

def test_405():
    status, reason, body, headers = request("DELETE", "/")
    print_result("405 Method Not Allowed", status == 405, f"Status: {status}, Reason: {reason}")

def test_411():
    status, reason, body, headers = request("POST", "/upload", headers={"Content-Type": "text/plain"})
    print_result("411 Length Required", status == 411, f"Status: {status}, Reason: {reason}")

def test_413():
    status, reason, body, headers = request("POST", "/upload", headers={"Content-Type": "text/plain", "Content-Length": "999999999"}, body="A")
    print_result("413 Payload Too Large", status == 413, f"Status: {status}, Reason: {reason}")

def test_expect_continue():
    status, reason, body, headers = request("POST", "/upload", headers={"Content-Length": "10"}, body="1234567890", expect_continue=True)
    print_result("Expect: 100-continue", status in (200, 201), f"Status: {status}, Reason: {reason}")

def test_expect_413():
    status, reason, body, headers = request("POST", "/", headers={"Content-Length": "999999999"}, body="A", expect_continue=True)
    print_result("Expect: 100-continue triggers 413", status == 413, f"Status: {status}, Reason: {reason}")

def test_chunked():
    status, reason, body, headers = request("POST", "/upload", use_chunked=True, body="chunkeddata")
    print_result("POST chunked", status in (200, 201), f"Status: {status}, Reason: {reason}")

def test_chunked_with_cl():
    status, reason, body, headers = request("POST", "/upload", headers={"Content-Length": "10"}, use_chunked=True, body="chunkeddata")
    print_result("Chunked with Content-Length", status == 400, f"Status: {status}, Reason: {reason}")

def test_invalid_te():
    status, reason, body, headers = request("POST", "/upload", headers={"Transfer-Encoding": "gzip"}, body="test")
    print_result("Invalid Transfer-Encoding", status == 501, f"Status: {status}, Reason: {reason}")

def test_400():
    # Malformed request: missing HTTP version
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((SERVER, PORT))
    s.sendall(b"GET /missing_http_version\r\n\r\n")
    resp = s.recv(4096).decode(errors="replace")
    s.close()
    passed = "400 Bad Request" in resp
    print_result("400 Bad Request (malformed)", passed, resp)

def test_505():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((SERVER, PORT))
    s.sendall(b"GET /upload HTTP/0.9\r\nHost: localhost:8080\r\n\r\n")
    resp = s.recv(4096).decode(errors="replace")
    s.close()
    passed = "505 HTTP Version Not Supported" in resp
    print_result("505 HTTP Version Not Supported", passed, resp)
    
def test_501():
    status, reason, body, headers = request("PATCH", "/")
    print_result("501 Not Implemented", status == 501, f"Status: {status}, Reason: {reason}")
    
def run_all_tests():
    setup_test_file()
    test_simple_get()
    test_not_found()
    test_post_upload()
    test_delete()
    cleanup_test_file()
    test_405()
    test_411()
    test_413()
    test_expect_continue()
    test_expect_413()
    test_chunked()
    test_chunked_with_cl()
    test_invalid_te()
    test_400()
    test_505()
    test_501()
    print("All tests completed.")

if __name__ == "__main__":
    run_all_tests()