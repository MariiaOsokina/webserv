#!/usr/bin/php-cgi
<?php
// Send headers immediately
header("Content-Type: text/plain");
echo "Starting a 10 second nap...\n";

// Sleep for 10 seconds to simulate heavy processing
sleep(10);

echo "I woke up!\n";
?>