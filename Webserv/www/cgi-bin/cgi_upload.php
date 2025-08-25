<?php
#!/usr/bin/php
header("Content-Type: text/html; charset=utf-8");

// Check if a file was uploaded
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['file'])) {
    $uploadDir = dirname(__DIR__) . '/upload/';
    if (!is_dir($uploadDir)) {
        mkdir($uploadDir, 0777, true);
    }
    $filename = basename($_FILES['file']['name']);
    $targetPath = $uploadDir . $filename;

    if (move_uploaded_file($_FILES['file']['tmp_name'], $targetPath)) {
        echo "<html><body>";
        echo "<h2>Upload successful!</h2>";
        echo "<p>File saved as: " . htmlspecialchars($filename) . "</p>";
        echo "</body></html>";
    } else {
        echo "<html><body><h2>Upload failed!</h2></body></html>";
    }
} else {
    // Show upload form
    echo '<html><body>
        <form method="POST" enctype="multipart/form-data">
            <input type="file" name="file" />
            <input type="submit" value="Upload" />
        </form>
    </body></html>';
}
?>