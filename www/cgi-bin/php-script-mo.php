#!/usr/bin/php-cgi
<?php
echo "Content-Type: text/html\r\n\r\n";
echo "<h1>CGI Post Result</h1>";

// Read the raw body from stdin (fed by your pipe_in[1])
$raw_body = file_get_contents("php://input");
echo "<p>Raw Body received: <b>" . $raw_body . "</b></p>";

// Show parsed POST data
echo "<p>Parsed Message: " . ($_POST['message'] ?? 'None') . "</p>";
?>
