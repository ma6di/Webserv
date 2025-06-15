<?php
echo "<h1>Hello from PHP CGI!</h1>";
echo "<p>Request method: " . $_SERVER['REQUEST_METHOD'] . "</p>";
echo "<p>Query string: " . $_SERVER['QUERY_STRING'] . "</p>";
?>