#!/usr/bin/php-cgi
<?php
// PHP-CGI automatically generates the "Content-Type: text/html" header, 
// but we can explicitly set it using the header() function if needed.

echo "<html><head><title>POST Test</title></head><body>\n";
echo "<h1>PHP CGI POST Test Successful!</h1>\n";

// 1. Check the parsed $_POST array
// php-cgi will automatically parse the body into $_POST *IF* your C++ server 
// provided the correct CONTENT_TYPE and CONTENT_LENGTH environment variables.
$message = isset($_POST['message']) ? htmlspecialchars($_POST['message']) : "<i>No message found in \$_POST</i>";

echo "<h3>Parsed Form Data:</h3>\n";
echo "<p><strong>Message:</strong> " . $message . "</p>\n";

// 2. Read the RAW body from stdin
// This proves whether your C++ write() to pipe_in actually worked.
$raw_body = file_get_contents("php://input");

echo "<h3>Raw Standard Input (pipe_in):</h3>\n";
if (empty($raw_body)) {
    echo "<pre>[STDIN IS EMPTY]</pre>\n";
} else {
    echo "<pre>" . htmlspecialchars($raw_body) . "</pre>\n";
}

// 3. Print essential Environment Variables
// These are strictly required by CGI/1.1 for POST requests to work
echo "<h3>Important Environment Variables:</h3>\n";
echo "<ul>\n";
$env_vars = ['REQUEST_METHOD', 'CONTENT_LENGTH', 'CONTENT_TYPE'];
foreach ($env_vars as $var) {
    $value = isset($_SERVER[$var]) ? $_SERVER[$var] : "<b>MISSING</b>";
    echo "<li><strong>$var:</strong> $value</li>\n";
}
echo "</ul>\n";

echo "</body></html>\n";
?>