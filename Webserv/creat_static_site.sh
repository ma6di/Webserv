#!/bin/bash

set -e

STATIC_DIR="www/static"
UPLOAD_DIR="www/upload"
CGI_DIR="www/cgi-bin"

GREEN='\033[0;32m'
CYAN='\033[1;36m'
NC='\033[0m'

divider() {
    echo -e "${CYAN}------------------------------------------------------------${NC}"
}

echo
divider
echo -e "${GREEN}Creating static website structure...${NC}"
divider

# Create directories
mkdir -p "$STATIC_DIR/js" "$STATIC_DIR/images" "$UPLOAD_DIR" "$CGI_DIR"

# index.html
cat > "$STATIC_DIR/index.html" <<EOF
<!DOCTYPE html>
<html>
<head>
    <title>Welcome to Webserv Static Site</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1>Welcome to Webserv Static Site</h1>
    <p>This is the home page served by your C++ web server.</p>
    <img src="images/logo.png" alt="Logo" width="128">
    <p><a href="about.html">About</a></p>
    <script src="js/app.js"></script>
</body>
</html>
EOF

# about.html
cat > "$STATIC_DIR/about.html" <<EOF
<!DOCTYPE html>
<html>
<head>
    <title>About - Webserv Static Site</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <h1>About This Site</h1>
    <p>
        <b>Webserv</b> is a lightweight, educational HTTP server written in C++.<br>
        It supports static file serving, CGI scripts, file uploads, and custom error pages.<br>
        This demo site shows static content and basic navigation.
    </p>
    <h2>Contact</h2>
    <ul>
        <li>Email: <a href="mailto:your@email.com">your@email.com</a></li>
        <li>GitHub: <a href="https://github.com/yourrepo" target="_blank">yourrepo</a></li>
    </ul>
    <p><a href="index.html">Home</a></p>
</body>
</html>
EOF

# style.css
cat > "$STATIC_DIR/style.css" <<EOF
body { font-family: Arial, sans-serif; background: #f8f8f8; color: #222; margin: 2em; }
h1 { color: #3366cc; }
a { color: #3366cc; text-decoration: none; }
a:hover { text-decoration: underline; }
img { border-radius: 8px; }
ul { margin-top: 1em; }
EOF

# js/app.js
cat > "$STATIC_DIR/js/app.js" <<EOF
document.addEventListener('DOMContentLoaded', function() {
    console.log("Webserv static site loaded!");
});
EOF

# images/logo.png (placeholder: use a 1x1 transparent PNG)
base64 -d > "$STATIC_DIR/images/logo.png" <<EOF
iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAAFUlEQVR42mP8z/C/HwAE/wH+gQEAOwAAAP//AwB6A8kAAAAASUVORK5CYII=
EOF

# 404.html
cat > "www/404.html" <<EOF
<!DOCTYPE html>
<html>
<head>
    <title>404 Not Found</title>
    <style>
        body { font-family: Arial; background: #fff0f0; color: #a00; text-align: center; margin-top: 5em; }
        a { color: #3366cc; }
    </style>
</head>
<body>
    <h1>404 Not Found</h1>
    <p>The requested resource could not be found on this server.</p>
    <p>If you believe this is an error, please <a href="mailto:your@email.com">contact us</a>.</p>
    <a href="/static/index.html">Go to Home</a>
</body>
</html>
EOF

divider
echo -e "${GREEN}Static website files created in $STATIC_DIR${NC}"
echo -e "${GREEN}Upload directory: $UPLOAD_DIR${NC}"
echo -e "${GREEN}CGI directory: $CGI_DIR${NC}"
divider