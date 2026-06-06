#!/usr/bin/php-cgi
<?php
// Note: php-cgi automatically outputs the Content-Type header for us.

echo "<html><body>\n";
echo "<h1>CGI POST Test (PHP)</h1>\n";

// 1. Show Environment Variables received from your C++ Server
$method = getenv("REQUEST_METHOD");
$len = getenv("CONTENT_LENGTH");
$type = getenv("CONTENT_TYPE");

echo "<h3>Environment Variables:</h3>\n";
echo "<ul>\n";
echo "<li>REQUEST_METHOD: $method</li>\n";
echo "<li>CONTENT_LENGTH: $len</li>\n";
echo "<li>CONTENT_TYPE: $type</li>\n";
echo "</ul>\n";

// 2. Read raw stdin to prove the C++ pipe is working exactly as sent
$raw_input = file_get_contents("php://input");
echo "<h3>Raw Stdin Received from Pipe:</h3>\n";
echo "<pre>[" . htmlspecialchars($raw_input) . "]</pre>\n";

// 3. Check parsed $_POST variables 
// (If this is empty but raw stdin has data, your CONTENT_TYPE header is missing/wrong!)
$username = isset($_POST['username']) ? htmlspecialchars($_POST['username']) : "Not provided";
$password = isset($_POST['password']) ? htmlspecialchars($_POST['password']) : "Not provided";

echo "<h3>Parsed Data:</h3>\n";
echo "<ul>\n";
echo "<li>Username: <strong>$username</strong></li>\n";
echo "<li>Password: <strong>$password</strong></li>\n";
echo "</ul>\n";

echo "</body></html>\n";
?>