import socket
import time
import argparse
import random
import sys

# --- CONFIGURATION ---
SERVER_HOST = '127.0.0.1'
SERVER_PORT = 8080
TIMEOUT = 5.0  # Increased for churn/slow tests

# --- UTILITY FUNCTIONS ---
def get_socket():
	"""Helper to create and connect a socket with standard timeouts."""
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.settimeout(TIMEOUT)
	try:
		s.connect((SERVER_HOST, SERVER_PORT))
	except socket.error as e:
		print(f"Error: Is {SERVER_HOST}:{SERVER_PORT} running?")
		print(f"       {e}")
		sys.exit(1)
	return s

def print_result(expected, actual, passed, error=None):
	"""Standardizes the pass/fail output format."""
	if error:
		print(f"   ❌ ERROR: {error}")
	elif passed:
		print(f"   ✅ PASS ({actual})")
	else:
		print(f"   ❌ FAIL (Expected {expected}, got: {actual})")
	print("-" * 50)

# --- STANDARD TEST RUNNER ---
def run_test(category, test_name, raw_bytes, expected_status, fragment=False):
	print(f"[{category}] {test_name}")
	try:
		with get_socket() as s:
			if fragment:
				mid = len(raw_bytes) // 2
				s.sendall(raw_bytes[:mid])
				time.sleep(0.5)
				s.sendall(raw_bytes[mid:])
			else:
				s.sendall(raw_bytes)

			response = s.recv(4096).decode('utf-8', errors='ignore')
			first_line = response.split('\r\n')[0] if response else "EMPTY RESPONSE"
			
			passed = expected_status in first_line
			print_result(expected_status, first_line, passed)
			
	except Exception as e:
		print_result(expected_status, "ERROR", False, error=str(e))

# --- COMPLEX / CUSTOM TESTS ---

def test_stalled_zero_chunk():
	print("[STRESS] Stalled Zero Chunk (Chunked Encoding Termination)")
	try:
		with get_socket() as s:
			# Start a chunked request
			s.sendall(b"POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n")
			s.sendall(b"5\r\nHello\r\n")
			
			# Send the zero chunk but MISS the final \r\n
			# The server should still be waiting for the end of the trailer/body
			s.sendall(b"0\r\n") 
			print("   -> Sent '0\r\n', waiting to see if server holds...")

			s.settimeout(1.0)
			try:
				response = s.recv(4096)
				if response:
					print_result("Wait", f"Premature response: {response[:15]}", False)
					return
			except socket.timeout:
				# This is the expected behavior; the server is correctly waiting
				print("   -> Server is correctly stalled (Timeout hit).")
				pass 

			# Now send the final CRLF to complete the request
			s.sendall(b"\r\n") 
			print("   -> Sent final CRLF.")
			
			s.settimeout(TIMEOUT) # Reset to your standard global timeout
			response = s.recv(4096).decode('utf-8', errors='ignore')
			
			passed = "201" in response
			status = response.split('\r\n')[0] if response else "No Response"
			
			print_result("201 OK after CRLF", status, passed)

	except Exception as e:
		print_result("201 Created", "ERROR", False, error=str(e))

def test_fragmented_chunked():
	print("[STRESS] Fragmented Chunked Body...", end=" ", flush=True)
	try:
		with get_socket() as s:
			s.sendall(b"POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n")
			# Send '5\r\nHello\r\n' slowly
			for char in [b'5', b'\r', b'\n', b'H', b'e', b'l', b'l', b'o', b'\r', b'\n']:
				s.sendall(char)
				time.sleep(0.05)
			s.sendall(b"0\r\n\r\n")
			response = s.recv(4096).decode()
			if "201" in response:
				print("✅ PASS")
			else:
				print("❌ FAIL")
	except Exception as e:
		print(f"❌ ERROR: {e}")


def test_partial_read():
	print("[ADVANCED] Partial Read (Byte-by-Byte)")
	request = b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n0123456789"
	try:
		with get_socket() as s:
			for byte in request:
				s.send(bytes([byte]))
				time.sleep(0.01)
			response = s.recv(4096).decode('utf-8', errors='ignore')
			first_line = response.split('\r\n')[0] if response else "EMPTY"
			print_result("201", first_line, "201" in first_line)
	except Exception as e:
		print_result("201", "ERROR", False, error=str(e))

def test_fragmented_standard_body():
	print("[STRESS] Fragmented Standard Body (Partial Body Sends)")
	try:
		with get_socket() as s:
			# Content-Length is 10
			# Send headers first
			s.sendall(b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n")
			
			# Send first half of the body
			s.sendall(b"12345") 
			print("   -> Sent first 5 bytes. Waiting...")
			
			time.sleep(0.1) # Wait for server to handle partial read/POLLIN
			
			# Send second half of the body
			s.sendall(b"67890") 
			print("   -> Sent remaining 5 bytes.")
			
			# Receive response
			response = s.recv(4096).decode('utf-8', errors='ignore')
			
			passed = "201" in response
			status = response.split('\r\n')[0] if response else "No Response"
			
			print_result("201 Created", status, passed)
			
	except Exception as e:
		print_result("201 Created", "ERROR", False, error=str(e))



def test_keep_alive():
	print("[ADVANCED] Keep-Alive (Multiple Requests on One Socket)")
	try:
		with get_socket() as s:
			s.sendall(b"GET /index-mo.html HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n")
			resp1 = s.recv(4096).decode('utf-8', errors='ignore')
			if "200" not in resp1:
				print_result("200", "First Fail", False)
				return
			time.sleep(0.2)
			s.sendall(b"GET /index-mo.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
			resp2 = s.recv(4096).decode('utf-8', errors='ignore')
			print_result("200", resp2.split('\r\n')[0], "200" in resp2)
	except Exception as e:
		print_result("200", "ERROR", False, error=str(e))

def test_churn(num_conns=20):
	print(f"[ADVANCED] Connection Churn (Stress Test {num_conns} FDs)")
	sockets = []
	try:
		for i in range(num_conns):
			sockets.append(get_socket())
		random.shuffle(sockets)
		for s in sockets:
			s.close()
			time.sleep(0.05)
		print_result("Success", "All Closed", True)
	except Exception as e:
		print_result("Success", "Failure", False, error=str(e))

def test_partial_send():
	"""Requires a large file or large response string on the server to trigger POLLOUT."""
	print("[ADVANCED] Partial Send (Slow Consumer - Checking POLLOUT)")
	try:
		with get_socket() as s:
			# Tell OS we have a tiny buffer to force TCP backpressure
			s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024) 
			s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
			
			# Read just a tiny bit and wait
			chunk = s.recv(10)
			print(f"   -> Received initial {len(chunk)} bytes. Server should be waiting in POLLOUT...")
			time.sleep(1.0) 
			
			remaining = s.recv(1000000)
			passed = len(remaining) > 0
			print_result("More Data", f"Total extra: {len(remaining)}", passed)
	except Exception as e:
		print_result("Data", "ERROR", False, error=str(e))


def test_body_with_pipelining():
    print("[STRESS] Body with Pipelined Request...", end=" ", flush=True)
    try:
        with get_socket() as s:
            request = (
                b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n"
                b"HELLO"
                b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
            )
            s.sendall(request)
            
            # Accumulated buffer for responses
            full_response = ""
            s.settimeout(2.0) # Don't wait forever
            
            # Keep reading until we have 2 HTTP headers
            while full_response.count("HTTP/1.1") < 2:
                chunk = s.recv(8192).decode()
                if not chunk: # Connection closed
                    break
                full_response += chunk

            if full_response.count("HTTP/1.1") >= 2:
                print("✅ PASS (Both requests processed)")
            else:
                print(f"❌ FAIL (Found {full_response.count('HTTP/1.1')} responses)")
                
    except Exception as e:
        print(f"❌ ERROR: {e}")
		
def test_pipelining():
	print("[ADVANCED] HTTP Pipelining...", end=" ", flush=True)
	# Explicitly ask for keep-alive
	reqs = (
		b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
		b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
		b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
	)
	try:
		with get_socket() as s:
			s.sendall(reqs)
			
			full_response = ""
			start_time = time.time()
			
			# Loop until we get 3 responses or 2 seconds pass
			while full_response.count("HTTP/1.1") < 3 and (time.time() - start_time) < 2:
				chunk = s.recv(4096).decode('utf-8', errors='ignore')
				if not chunk: break
				full_response += chunk

			count = full_response.count("HTTP/1.1")
			if count >= 3:
				print(f"✅ PASS (Found {count} responses)")
			else:
				print(f"❌ FAIL (Found {count}/3 responses)")
	except Exception as e:
		print(f"❌ ERROR: {e}")

def test_body_with_pipelining():
	print("[STRESS] Body with Pipelined Request")
	request = (
		b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n"
		b"HELLO"
		b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
	)
	try:
		with get_socket() as s:
			s.sendall(request)
			response = s.recv(8192).decode('utf-8', errors='ignore')
			occurrences = response.count("HTTP/1.1")
			passed = (occurrences >= 2)
			print_result("2 Responses", f"Found {occurrences} responses", passed)
	except Exception as e:
		print_result("2 Responses", "ERROR", False, error=str(e))


def test_slowloris():
	print("[SECURITY] Slowloris (Header Timeout Test)")
	try:
		with get_socket() as s:
			s.sendall(b"GET / HTTP/1.1\r\n")
			s.sendall(b"Host: localhost\r\n")
			print("   -> Sending headers partially... waiting for timeout.")
			
			# Must be longer than your server's CONNECTION_TIMEOUT
			time.sleep(11) 
			
			try:
				response = s.recv(4096).decode('utf-8', errors='ignore')
				# Success if server sent 408 Request Timeout OR closed connection (empty response)
				passed = "408" in response or not response
				status = response.split('\r\n')[0] if response else "Socket Closed"
				print_result("408 or Close", status, passed)
			except socket.error:
				print_result("Close", "Socket Closed by Peer", True)
	except Exception as e:
		print_result("Timeout", "ERROR", False, error=str(e))


def test_408_timeout():
	print("[TIMEOUT] Testing 408 Request Timeout (Incomplete Headers)")
	try:
		with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
			# High socket timeout so the script doesn't crash before the server responds
			s.settimeout(100) 
			s.connect(('127.0.0.1', 8080))
			
			# Send an incomplete header (Missing the double \r\n\r\n)
			s.sendall(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n")
			
			print(f"   -> Sent partial headers. Waiting for server's internal timeout...")
			
			start_time = time.time()
			# The server should notice the idle connection and eventually send a 408
			response = s.recv(4096).decode('utf-8', errors='ignore')
			end_time = time.time()
			
			duration = end_time - start_time
			passed = "408" in response
			status = response.split('\r\n')[0] if response else "Socket Closed"
			
			print_result("408 Request Timeout", f"{status} (after {duration:.2f}s)", passed)
				
	except Exception as e:
		print_result("408 Request Timeout", "ERROR", False, error=str(e))

def test_verify_408():
	addr = ('127.0.0.1', 8080)
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
		s.settimeout(100) # Total script timeout
		s.connect(addr)
		
		# Send partial request
		s.sendall(b"GET / HTTP/1.1\r\nHost: localhost") 
		print("Sent partial request. Waiting for 408...")
		
		start = time.time()
		try:
			response = s.recv(4096).decode()
			duration = time.time() - start
			
			if "408" in response:
				print(f"✅ SUCCESS: Received 408 Request Timeout in {duration:.2f}s")
				print(f"Response Headers:\n{response.splitlines()[0]}")
			else:
				print(f"❌ FAIL: Received different response: {response[:20]}")
		except socket.timeout:
			print("❌ FAIL: Script timed out before server sent 408.")
		except ConnectionResetError:
			print("INFO: Server closed connection without sending a 408 body (Common in Nginx).")

# --- MAIN EXECUTION ---
if __name__ == "__main__":
	parser = argparse.ArgumentParser(description="WebServ Test Suite")
	parser.add_argument("--host", default=SERVER_HOST, help="Server IP")
	parser.add_argument("--port", type=int, default=SERVER_PORT, help="Server Port")
	args = parser.parse_args()

	SERVER_HOST, SERVER_PORT = args.host, args.port
	print(f"Starting WebServ Test Suite on {SERVER_HOST}:{SERVER_PORT}\n")

	# 1. Request Line & Headers
	run_test("REQ_LINE", "Valid Request", b"GET /index-mo.html HTTP/1.1\r\nHost: localhost\r\n\r\n", "200")
	run_test("REQ_LINE", "Valid Request", b"GET /index-mo.html HTTP/1.1\r\nHost: localhost\r\n\r\n", "200")
	run_test("REQ_LINE", "Missing Version", b"GET /\r\nHost: localhost\r\n\r\n", "400")
	run_test("HEADERS", "Duplicate Host (Illegal)", b"GET /index-mo.html HTTP/1.1\r\nHost: a.com\r\nHost: b.com\r\n\r\n", "400")
	run_test("HEADERS", "Duplicate Content-Length", b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 10\r\n\r\nHello", "400")

	# 2. Protocol Strictness
	run_test("PROTOCOL", "Missing Host Header", b"GET / HTTP/1.1\r\n\r\n", "400")
	run_test("PROTOCOL", "HTTP 2.0 Not Supported", b"GET / HTTP/2.0\r\nHost: localhost\r\n\r\n", "505")
	run_test("PROTOCOL", "Method Not Allowed", b"DELETE /index-mo.html HTTP/1.1\r\nHost: localhost\r\n\r\n", "405")
	run_test("PROTOCOL", "HEAD Request for / root", b"HEAD / HTTP/1.1\r\nHOST: localhost\r\n\r\n", "200")
	run_test("PROTOCOL", "HEAD Request for /nonexistent", b"HEAD /nonexistent HTTP/1.1\r\nHOST: localhost\r\n\r\n", "404")
	run_test("PROTOCOL", "GET Request mixed-case headers", b"GET /index-mo.html HTTP/1.1\r\nhOSt: localhost\r\nCOnTEnt-leNGTH: 5\r\n\r\nHello", "200")

	# 3. Limits & Security

	huge_header = b"X-Junk: " + (b"A" * 17000) + b"\r\n"
	
	run_test("LIMITS", "431 Header Too Large", b"GET / HTTP/1.1\r\nHost: localhost\r\n" + huge_header + b"\r\n", "431")
	run_test("LIMITS", "413 Payload Too Large", b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10000000\r\n\r\n" + (b"A" * 100), "413")


	# Exact match
	run_test("BODY", "Exact Content-Length", 
	         b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n12345", "201")

	# Content-Length: 0 (Should transition to REQUEST_READY immediately)
	run_test("BODY", "Zero Content-Length", 
	         b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", "201")

	# Content-Length larger than max_body_size (Should be caught in _setupBodyReading)
	# Assuming your max_body_size in config is smaller than 1,000,000,000
	run_test("SECURITY", "Massive Content-Length", 
	         b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000000000\r\n\r\n", "413")


	#1. SECURITY / SMUGGLING
 
	run_test("SECURITY", "Smuggling (CL + TE)", b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", "400")
	run_test("SECURITY", "Invalid Hex Chunk Size", 

	         b"POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nG\r\n", "400")
	# 2. CHUNKED VARIATIONS
	run_test("BODY", "Standard Chunked", 
	         b"POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n", "201")
	
	run_test("BODY", "Chunked Extensions", 
	         b"POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=val\r\nHello\r\n0\r\n\r\n", "201")
	
	run_test("BODY", "Multiple Trailers", 
	     b"POST /upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
	     b"5\r\nHello\r\n"
	     b"0\r\n"
	     b"Trailer-One: value1\r\n"
	     b"Trailer-Two: value2\r\n"
	     b"\r\n", "201")

	# 3. TRANSFER ENCODING LIMITS
	run_test("BODY", "Unsupported TE (gzip)", 
	         b"POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\ndata", "400")
	
	run_test("BODY", "TE: identity", 
	         b"POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: identity\r\nContent-Length: 5\r\n\r\nHello", "400")

	# 4. STRESS & TIMING
	test_stalled_zero_chunk()
	test_fragmented_chunked()

	#5. State Machine & Stress
	test_keep_alive()
	test_partial_read()
	test_fragmented_standard_body()
	test_partial_send()
	# test_churn(30)
	test_pipelining()
	##test_body_with_pipelining() //test with nc
	test_slowloris()
	test_verify_408()

	test_408_timeout()
	print("All tests complete!")
	