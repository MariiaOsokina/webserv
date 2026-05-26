# 🚀 WebServ CGI Testing
This guide provides a standardized set of tests to verify that our WebServ correctly handles CGI execution (PHP and Python).

---

## 0. Environment Setup 🛠️
To run the CGI tests, your system must have the correct interpreters installed.

### Linux (Ubuntu/Debian)
```bash
sudo apt update && sudo apt install php-cgi python3 -y
```

### macOS
```bash
brew install php python3
```

*Verify installation:* ```bash
php-cgi -v 
python3 --version
```

> **Note:** If `php-cgi` is in a non-standard path, update the `cgi_pass` in `config/advanced.conf`.

---

## 1. Prerequisites
* **Compile:** ```bash
  make re
  ```
* **Launch:** Start the server with the advanced configuration:
  ```bash
  ./webserv config/advanced.conf
  ```

---

## 2. Integration Tests (Browser-Based)
These tests verify navigation, static routing, and basic CGI GET functionality.

### Step 1: The Hub
* **Navigate to:** `http://localhost:8080/index-ade.html`
* **Verification:** The page should load with a feature list and navigation links.

### Step 2: Static & CGI GET
* **Browse Files:** Click the "Browse Files" link.
  * *Expect:* Directory listing or contents of the `/files` folder.
* **PHP CGI Test:** Click the "PHP CGI Test" link (`test.php`).
  * *Expect:* A PHP-generated page showing `REQUEST_METHOD: GET`.

---

## 3. CGI GET Tests (Python)
Verify query string parsing and environment variable passing.

**Command:**
```bash
curl -i "http://localhost:8080/py-cgi/hello-mo.py?name=Anna&age=15"
```
* **Expect:** Response should greet "Anna" and display "15".

---

## 4. CGI POST Tests (PHP)
This section verifies that the server correctly pipes the request body to a CGI script.

### Step 1: Browser-Based POST Test
1. **Open the Test Page:** Navigate to `http://localhost:8080/index-mo.html` in your browser.
2. **Action:** Enter a message in the text field (e.g., "Hello Webserv!") and click "Send to CGI".
3. **Expectation:** The server should route the request to `test_post-mo.php`. The resulting page should display your message under "Parsed Form Data".

### Step 2: Automated POST Tests (Command Line)
For deep verification of body parsing and different encodings:

**Standard URL-Encoded POST:**
```bash
curl -v -X POST http://localhost:8080/cgi-bin/test_post-mo.php \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "message=HelloWebserv"
```

**Chunked Transfer Encoding POST:**
```bash
curl -v -X POST http://localhost:8080/cgi-bin/test_post-mo.php \
     -H "Transfer-Encoding: chunked" \
     -d "This data is sent in chunks!"
```

---

## 5. Stress & Edge Cases (Pipelining)
To test if the server handles multiple requests on a single connection (Pipelining):

```bash
python3 -c 'print("POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nHELLOGET /index-mo.html HTTP/1.1\r\nHost: localhost\r\n\r\n", end="")' | nc localhost 8080
```


## 6. Zombie Process & Connection Drop Test
 to verify that our WebServ correctly cleans up orphaned CGI processes when a client abruptly disconnects. This is a critical edge case evaluated in the 42 webserv project to prevent Process ID (PID) exhaustion.

### 1. Ensure the script has execute permissions:

```bash
chmod +x www/cgi-bin/sleep.php
```


### 2. The Attack (Abrupt Disconnect)
You will need two separate terminal windows to properly simulate and monitor this test.

Terminal 1 (The Server):
Start your web server normally.

```bash
./webserv config/advanced.conf
```
Terminal 2 (The Client):
Run the following curl command. IMMEDIATELY press Ctrl+C as soon as you press Enter.

```bash
curl http://localhost:8080/cgi-bin/sleep.php
```
Why? This simulates a client opening a heavy page and abruptly closing their browser tab while the server is still executing the PHP script in the background.

### 3. The Investigation
Immediately after pressing Ctrl+C in Terminal 2, run this command to check your system's running processes:

```bash
ps aux | grep php-cgi
```
(If you want to look specifically for zombie processes across your whole system, you can use: ps -ef | grep 'Z')

### 4. How to Read the Results
✅ PASS (Clean Getaway)
The moment you kill curl, the server detects the closed socket and destroys the Connection object. The destructor safely sends SIGKILL to the child CGI process and calls waitpid() to wipe its record.

Observation: Running ps aux | grep php-cgi returns nothing (except the grep command itself). The process was instantly assassinated. No PID leaks!

❌ FAIL (The Nightmare)
If the server's process management is flawed, the OS doesn't know the C++ object was destroyed.

Observation: You will see the php-cgi process still actively running in the background even though curl is closed.

The Zombie: After the 10 seconds finish, running the ps command will show the process marked as [php-cgi] <defunct>. This is a Zombie process that will permanently lock up a PID until the web server shuts down.