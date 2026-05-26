#!/usr/bin/env python3
import os
import urllib.parse

# 1. Output the required HTTP headers FIRST
# The empty print("") creates the \r\n\r\n boundary between headers and body
print("Status: 200 OK")
print("Content-Type: text/html")
print("") 

# 2. Start HTML Body
print("<html><body>")
print("<h1>CGI GET Test (Python)</h1>")

print("<h1>Instructions</h1>")
print("<p><div>Edit the URL in your browser, appending ? followed by a pair like variable=value</div><div>Variables can be separated by & see the example below:</div><div>?todays_plan=relax&name=Joanna&age=42</div></p>")

# 3. Read the QUERY_STRING from the environment
query_string = os.environ.get("QUERY_STRING", "NO_QUERY_STRING_FOUND")
print(f"<h3>Raw Query String:</h3> <p><code>{query_string}</code></p>")

# 4. Parse the variables
parsed_params = urllib.parse.parse_qs(query_string)
name = parsed_params.get("name", ["Unknown"])[0]
age = parsed_params.get("age", ["Unknown"])[0]

print("<h3>Parsed Data:</h3>")
print(f"<ul><li><strong>Name:</strong> {name}</li><li><strong>Age:</strong> {age}</li></ul>")

print("</body></html>")