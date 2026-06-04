/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServ.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:03:57 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 16:48:07 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServ.hpp"

/* public section ----------------------------- */

WebServ::WebServ()
{
}

WebServ::~WebServ()
{
	// 1. Servers (Static)
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		delete _listeners[i]; // Triggers ~Listener() -> close(_listenFd)
	}
	_listeners.clear();
	// 2. Connections (Dynamic)
	std::map<int, Connection *>::iterator it;
	for (it = _fdToConnMap.begin(); it != _fdToConnMap.end(); ++it)
	{
		delete it->second; // Triggers ~Connection() -> close(_connectFd)
	}
	_fdToConnMap.clear();
	_pollFds.clear();
	std::cout << "[WebServ] All sockets closed. Cleanup complete." << std::endl;
}

std::vector<Listener *> WebServ::getListeners() const
{
	return _listeners;
}

void WebServ::setup(const std::vector<ServerConfig> &configs)
{
	for (size_t i = 0; i < configs.size(); ++i)
	{
		// One Listener (socket/bind/listen) per (server block, port).
		// The poll loop keys on listen FDs, so reusing the same config
		// per port handles multiple `listen` directives with no other
		// plumbing changes.
		for (size_t p = 0; p < configs[i].ports.size(); ++p)
		{
			int port = configs[i].ports[p];
			Listener *newListener = NULL;
			try
			{
				newListener = new Listener(configs[i], port);
				newListener->initSocket(); // Creates the socket, bind, listen
				int listenFd = newListener->getListenFd();

				_listeners.push_back(newListener);
				_addNewFdtoPool(listenFd, POLLIN);
				_fdToListenerMap[listenFd] = newListener;
				std::cout << "[WebServ] New Listener setted up on FD: " << listenFd << std::endl; // log
			}
			catch (const std::exception &e)
			{
				std::cerr << "Failed to setup listener " << configs[i].host << ":" << port << ": " << e.what() << std::endl;
				if (newListener)
					delete newListener;
			}
		}
	}
}

/* Event dispatch priority per FD:
   1. CGI pipes (internal) — read() output or handle termination
   2. POLLIN — recv() data, or 0 means client closed
   3. POLLOUT — flush queued response
   4. POLLERR/POLLHUP/POLLNVAL — fallback for abnormal errors */

void WebServ::run(void)
{
	time_t lastTimeoutCheck = std::time(NULL);

	while (g_server_running)
	{
		time_t now = std::time(NULL);
		// Only run the heavy O(N^2) check once per second
		if (now - lastTimeoutCheck >= 1)
		{
			_checkConnTimeouts();
			_checkCGITimeouts();
			lastTimeoutCheck = now;
		}
		int ret = poll(&_pollFds[0], _pollFds.size(), POLL_TIMEOUT);
		if (ret == 0)
			continue; // Poll Timeout
		// Handle Poll Errors
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;			   // Interrupted by other system calls than Ctrl + C
			if (g_server_running == 0) // Interrupted by Ctrl + C
				break;
			throw std::runtime_error(std::string("Poll failed: ") + std::strerror(errno));
		}
		// Loop through FDs
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents == 0)
				continue;

			// 1. HANDLE CGI ---
			if (_isCGIStdinFd(_pollFds[i].fd))
			{
				_handleCGIInput(&i);
				continue;
			}
			if (_isCGIStdoutFd(_pollFds[i].fd))
			{
				if (_pollFds[i].revents & (POLLIN | POLLHUP))
				{
					_handleCGIOutput(&i);
				}
				else if (_pollFds[i].revents & (POLLERR | POLLNVAL))
				{
					std::cout << "[WebServ] CGI fd: " << _pollFds[i].fd << " removed due to error" << std::endl;
					_cleanupCGI(_pollFds[i].fd); // already swap-pops the fd out of _pollFds
					i--;						 // re-examine whatever got swapped into slot i
				}
				continue;
			}

			// 2. HANDLE READ
			if (_pollFds[i].revents & POLLIN)
			{
				if (_isListener(_pollFds[i].fd))
					this->_acceptNewConnection(_pollFds[i].fd);
				else
					_readRequest(&i);
			}

			// 3. HANDLE WRITES
			else if (_pollFds[i].revents & POLLOUT)
			{
				_sendResponse(&i);
			}
			// 4. HANDLE ERRORS (Lowest Priority)
			// This is a fallback. Standard closures are usually handled by
			// recv() returning 0 in step 1. This catches abnormal errors.
			else if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				_closeConnection(i);
				i--;
			}
		}
	}
}

void WebServ::_addNewFdtoPool(int newFd, short events)
{
	if (newFd < 0)
		return;

	pollfd pfd;
	pfd.fd = newFd;
	pfd.events = events;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "[Poll] Added FD " << newFd << " to monitoring pool." << std::endl; // log
}

bool WebServ::_isListener(int fd)
{
	return _fdToListenerMap.find(fd) != _fdToListenerMap.end();
}

void WebServ::_acceptNewConnection(int listenFd)
{
	sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);
	// 1. ACCEPT CONNECTION
	int connFd = accept(listenFd, (sockaddr *)&clientAddr, &clientLen);
	if (connFd < 0)
	{
		std::cout << "[WebServ] Notice: Could not complete accept on FD " << listenFd << std::endl;
		return;
	}
	// 2. SET NON-BLOCKING
	if (setNonBlocking(connFd) == false)
	{
		close(connFd);
		connFd = -1;
		return;
	}
	// 3. CREAT CONNECTION OBJECT
	Connection *newConn = NULL;
	try
	{
		std::map<int, Listener *>::iterator it = _fdToListenerMap.find(listenFd);
		if (it == _fdToListenerMap.end())
		{
			std::cerr << "[WebServ] Critical Error: Listener FD " << listenFd << " not associated with any Listener." << std::endl;
			close(connFd);
			return;
		}
		Listener *listener = it->second;
		newConn = new Connection(connFd, listener); // can throw exception
		_addNewFdtoPool(connFd, POLLIN);
		_fdToConnMap[connFd] = newConn;
		std::cout << "[WebServ] New connection accepted on FD: " << connFd << " (Listener FD " << listenFd << ")" << std::endl; // log
	}
	catch (const std::exception &e)
	{
		std::cerr << "[WebServ] Failed to create connection: " << e.what() << std::endl;
		close(connFd);
	}
}

void WebServ::_closeConnection(size_t index)
{
	int fd = _pollFds[index].fd;

	// 1. Kill any CGI orphaned by this connection before destroying it.
	std::map<int, CGIProcess>::iterator it = _cgiProcesses.begin();
	while (it != _cgiProcesses.end())
	{
		if (it->second.connFd == fd)
		{
			std::cout << "[WebServ] Client dropped. Killing orphaned CGI PID: " << it->second.pid << std::endl;
			_terminateCGIProcess(it->second.stdoutFd);
			break; // helper invalidated 'it'
		}
		++it;
	}

	// 2. CLEAN UP CONNECTION
	if (_fdToConnMap.count(fd))
	{
		delete _fdToConnMap[fd];
		_fdToConnMap.erase(fd);
	}
	// 3. REMOVE FROM FD POOL (USING SWAP/POP, O(1) efficiency)
	size_t pollIdx = _findPollIndex(fd);
	if (pollIdx != static_cast<size_t>(-1))
	{
		_pollFds[pollIdx] = _pollFds.back();
		_pollFds.pop_back();
	}

	std::cout << "Closed connection on FD: " << fd << std::endl;
}

void WebServ::_readRequest(size_t *index)
{
	int fd = _pollFds[*index].fd;
	Connection *conn = _fdToConnMap[fd];

	// 1. ALWAYS try to process what's already in the buffer first.
	// This handles cases where a previous recv() got the end of the request.
	conn->handleRead(NULL, 0);

	// 2. Only call recv() if the request is NOT yet ready.
	if (conn->getState() != Connection::REQUEST_READY && conn->getState() != Connection::ERROR)
	{
		char tempBuffer[BUFFER_SIZE] = {0};
		int bytesRead = recv(fd, tempBuffer, sizeof(tempBuffer), 0);

		if (bytesRead > 0)
		{
			conn->resetTimeout();
			conn->handleRead(tempBuffer, bytesRead);
		}
		else if (bytesRead == 0)
		{
			std::cout << "[WebServ] Client closed connection on FD: " << fd << std::endl;
			_closeConnection(*index);
			(*index)--;
			return;
		}
		else
		{
			// We do NOT check errno != EAGAIN && errno != EWOULDBLOCK. We assume the connection is broken.
			// If poll() flags POLLIN, the kernel is promising you there is data to read (or the connection is closed).
			std::cerr << "[WebServ] Fatal recv error on FD: " << fd << ". Closing." << std::endl;

			_closeConnection(*index);
			(*index)--;
			return;
		}
	}

	// 3. Final State Check
	if (conn->getState() == Connection::REQUEST_READY || conn->getState() == Connection::ERROR)
	{
		std::cout << "[WebServ] Request Complete on FD " << fd << std::endl;
		// Routes the request and sets CGI flags if applicable.
		conn->prepareResponse();
		// CGI request (and no routing error): launch it non-blocking.
		if (conn->getState() != Connection::ERROR && conn->getResponse().isCGIGenerated())
		{
			std::string cgiExecPath = conn->getResponse().getCgiPath();
			std::string scriptPath = conn->getResponse().getScriptPath();

			std::cout << "[WebServ] Routing to CGI. Script: " << scriptPath << std::endl;
			_executeCGI(fd, cgiExecPath, scriptPath);

			// Return now — don't switch to POLLOUT; the CGI state machine
			// does that once the script produces output.
			return;
		}

		// Static response (non-CGI or error): send normally.
		std::cout << "[WebServ] Switching to POLLOUT for static response." << std::endl;
		_updateEvent(*index, POLLOUT, POLLIN);
	}
}

void WebServ::_sendResponse(size_t *index)
{
	int fd = _pollFds[*index].fd;
	std::map<int, Connection *>::iterator it = _fdToConnMap.find(fd);
	if (it == _fdToConnMap.end())
	{
		std::cerr << "[WebServ] Critical: No connection object for FD " << fd << std::endl;
		_closeConnection(*index);
		(*index)--;
		return;
	}
	Connection *conn = it->second;
	conn->resetTimeout();

	// NEW: Loop to flush all ready pipelined requests sequentially
	int requestsProcessed = 0;
	while (requestsProcessed < 10) // Limit to 10 requests per poll trigger
	{
		bool wasStreaming = (conn->getState() == Connection::STREAMING_CGI);
		bool finished = conn->handleWrite();

		// After each write, if we're in streaming mode, re-evaluate
		// back-pressure on the matching CGI's stdout. The buffer just
		// shrank, so we may want to re-enable POLLIN on the CGI side.
		if (wasStreaming && conn->getState() == Connection::STREAMING_CGI)
		{
			for (std::map<int, CGIProcess>::iterator cIt = _cgiProcesses.begin();
				 cIt != _cgiProcesses.end(); ++cIt)
			{
				if (cIt->second.connFd == fd)
				{
					_syncStreamPollFlags(cIt->second, conn);
					break;
				}
			}
		}

		if (finished)
		{
			requestsProcessed++;
			// A streaming response that ended in abort (post-header
			// crash, send() failure, etc.) MUST close — we can't
			// keep the connection alive after a truncated body.
			bool mustClose = conn->shouldClose() || conn->isStreamAborted();
			if (mustClose)
			{
				std::cout << "[WebServ] Closing connection on FD " << fd << std::endl;
				_closeConnection(*index);
				(*index)--;
				return;
			}
			else // keep open (Keep-Alive)
			{
				std::cout << "[WebServ] Keeping connection alive on FD " << fd << std::endl;
				conn->resetForNextRequest();
				_updateEvent(*index, POLLIN, POLLOUT);

				// CRITICAL FOR PIPELINING:
				if (conn->hasBufferedData())
				{
					std::cout << "[WebServ] Pipelined data found ("
							  << conn->getRawRequest().size() << " bytes)." << std::endl;

					_readRequest(index);

					// SAFETY CHECK: _readRequest might have encountered a fatal error and closed the connection
					if (_fdToConnMap.find(fd) == _fdToConnMap.end())
						return;

					// If a new response was fully prepared, loop to send it IMMEDIATELY
					// instead of waiting for the next poll() tick
					if (conn->getState() == Connection::REQUEST_READY || conn->getState() == Connection::ERROR)
						continue;
				}

				// No more complete requests ready to send, return to poll
				return;
			}
		}
		else
		{
			// In streaming mode, a "not finished" with empty buffer
			// just means "CGI hasn't fed us anything new yet". Drop
			// POLLOUT so the poll loop doesn't spin — _handleCGIOutput
			// will re-enable it when more bytes arrive.
			if (conn->getState() == Connection::STREAMING_CGI &&
				conn->streamBufferSize() == 0 &&
				!conn->isStreamFinished())
			{
				_setPollEvents(fd, 0, POLLOUT);
			}
			return;
		}
	}
}

void WebServ::_checkConnTimeouts()
{
	time_t now = std::time(NULL);
	std::map<int, Connection *>::iterator it = _fdToConnMap.begin();

	while (it != _fdToConnMap.end())
	{
		int fd = it->first;
		Connection *conn = it->second;
		std::map<int, Connection *>::iterator next = it;
		++next;

		if (conn->isTimedOut(now, CONNECTION_TIMEOUT))
		{
			// 1. Find the index in _pollFds once
			int pollIdx = -1;
			for (size_t i = 0; i < _pollFds.size(); ++i)
			{
				if (_pollFds[i].fd == fd)
				{
					pollIdx = i;
					break;
				}
			}

			if (pollIdx == -1)
			{
				it = next; // Should never happen, but safety first
				continue;
			}

			// 2. Decide: Polite Goodbye (408) or Silent Goodbye (Close)
			if (conn->getState() == Connection::READING_HEADERS ||
				conn->getState() == Connection::READING_BODY)
			{
				std::cout << "[WebServ] 408 Request Timeout on FD: " << fd << std::endl;
				conn->forceTimeoutError();
				conn->prepareResponse();

				// Flip to write mode to send the 408
				_updateEvent(pollIdx, POLLOUT, POLLIN);
			}
			// TO-DO: if the state is WAITING_FOR_CGI - send a 504 Gateway Timeout

			// MO: Let CGI timeout handler deal with hung scripts ??
			else if (conn->getState() == Connection::WAITING_FOR_CGI)
			{
				continue; // Do nothing here, _checkCGITimeouts() will handle it
			}
			else
			{
				std::cout << "[WebServ] Connection idle timeout (closing) on FD: " << fd << std::endl;
				_closeConnection(pollIdx);
			}
		}
		it = next;
	}
}

void WebServ::_updateEvent(size_t index, short enable, short disable)
{
	if (index >= _pollFds.size())
		return;

	// Turn ON the bits you want
	_pollFds[index].events |= enable;

	// Turn OFF the bits you don't want
	_pollFds[index].events &= ~disable;
}

bool WebServ::_isCGIStdoutFd(int fd) const
{
	return (_cgiProcesses.find(fd) != _cgiProcesses.end());
}

bool WebServ::_isCGIStdinFd(int fd) const
{
	return (_cgiStdinToStdout.find(fd) != _cgiStdinToStdout.end());
}

bool WebServ::_executeCGI(int connFd, const std::string &cgiPath, const std::string &scriptPath)
{
	Connection *conn = _fdToConnMap[connFd];

	CGILauncher cgi(cgiPath, scriptPath, conn->getRequest());

	// Start non-blocking CGI execution.
	CGIHandles h = cgi.executeNonBlocking();

	if (!h.ok())
	{

		std::cerr << "[WebServ] CGI fork failed for FD " << connFd << std::endl;
		conn->getResponse().setStatus(HTTP_Status::INTERNAL_SERVER_ERROR);
		conn->prepareResponse();
		conn->setState(Connection::ERROR); // MO: If fork fails, the request is dead. Set to ERROR

		// Find the client/connection in poll and set them to WRITE immediately
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].fd == connFd)
			{
				_updateEvent(i, POLLOUT, POLLIN);
				break;
			}
		}
		return false; // Fork failed.
	}
	conn->setCgiPid(h.pid);

	// Seed the CGI record. The body must outlive the request (drained to
	// stdin across many POLLOUT events), so swap it out (O(1)) instead of
	// copying — copying doubled RSS under big concurrent uploads and
	// caused OOM kills. The Connection no longer needs the body once the
	// CGI is running (the response comes from CGI output).
	CGIProcess process;
	process.pid = h.pid;
	process.stdoutFd = h.stdoutFd;
	process.stdinFd = h.stdinFd;
	process.connFd = connFd;
	conn->getRequest().swapBody(process.inputBody);
	process.inputOffset = 0;
	process.startTime = time(NULL);

	_cgiProcesses[h.stdoutFd] = process;

	// Fork succeeded: connection now waits on the CGI (not read/write).
	conn->setState(Connection::WAITING_FOR_CGI);
	_addNewFdtoPool(h.stdoutFd, POLLIN);

	// With a body, drain it via POLLOUT on stdin; without one, close the
	// write end now to send EOF immediately.
	if (process.inputBody.empty())
	{
		close(h.stdinFd);
		_cgiProcesses[h.stdoutFd].stdinFd = -1;
	}
	else
	{
		_cgiStdinToStdout[h.stdinFd] = h.stdoutFd;
		_addNewFdtoPool(h.stdinFd, POLLOUT);
	}

	return true;
}

// Closes the CGI stdin FD, unregisters it from the poll set and the
// stdin->stdout lookup, and clears cgi.stdinFd. Safe when already -1.
//
// cursor: the surrounding loop's index, or NULL. The swap+pop removal
// moves the back entry into the freed slot; if that entry was at/before
// the cursor, we decrement it so the loop stays consistent.
void WebServ::_closeCGIStdin(CGIProcess &cgi, size_t *cursor)
{
	int fd = cgi.stdinFd;
	if (fd < 0)
	{
		return;
	}
	close(fd);
	cgi.stdinFd = -1;
	_cgiStdinToStdout.erase(fd);
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds[i] = _pollFds.back();
			_pollFds.pop_back();
			if (cursor && *cursor != static_cast<size_t>(-1) && i <= *cursor)
			{
				(*cursor)--;
			}
			break;
		}
	}
}

void WebServ::_handleCGIInput(size_t *index)
{
	int stdinFd = _pollFds[*index].fd;

	std::map<int, int>::iterator lookup = _cgiStdinToStdout.find(stdinFd);
	if (lookup == _cgiStdinToStdout.end())
	{
		// Unknown stdin FD — defensive: remove from poll set.
		_pollFds[*index] = _pollFds.back();
		_pollFds.pop_back();
		(*index)--;
		return;
	}
	int stdoutFd = lookup->second;
	std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.find(stdoutFd);
	if (cgiIt == _cgiProcesses.end())
	{
		_pollFds[*index] = _pollFds.back();
		_pollFds.pop_back();
		_cgiStdinToStdout.erase(lookup);
		(*index)--;
		return;
	}
	CGIProcess &cgi = cgiIt->second;

	// Error on the pipe (e.g. CGI died before reading body) — abort cleanly.
	if (_pollFds[*index].revents & (POLLERR | POLLNVAL))
	{
		std::cerr << "[WebServ] CGI stdin pipe error (revents=" << _pollFds[*index].revents
				  << ") on FD " << stdinFd << std::endl;
		(*index)--;
		_cleanupCGI(stdoutFd);
		return;
	}

	size_t remaining = cgi.inputBody.size() - cgi.inputOffset;
	ssize_t n = write(stdinFd,
					  cgi.inputBody.data() + cgi.inputOffset,
					  remaining);

	if (n < 0)
	{
		// poll() said POLLOUT, so any error here (usually EPIPE — CGI
		// closed stdin early) is fatal; tear the whole thing down.
		std::cerr << "[WebServ] write() to CGI stdin failed on FD " << stdinFd << std::endl;
		(*index)--;
		_cleanupCGI(stdoutFd);
		return;
	}

	cgi.inputOffset += static_cast<size_t>(n);

	// startTime is a "last activity" watchdog: each stdin write / stdout
	// read pushes the CGI_TIMEOUT deadline forward, so steadily-progressing
	// large bodies aren't killed mid-transfer.
	cgi.startTime = time(NULL);

	if (cgi.inputOffset >= cgi.inputBody.size())
	{
		// Whole body written — close stdin to send EOF to the CGI.
		// _closeCGIStdin adjusts *index for us via the cursor argument.
		_closeCGIStdin(cgi, index);
	}
}

void WebServ::_handleCGIOutput(size_t *index)
{
	int cgiFd = _pollFds[*index].fd;

	std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.find(cgiFd);
	if (cgiIt == _cgiProcesses.end())
	{
		return;
	}

	CGIProcess &cgi = cgiIt->second;

	// Look up the owning client connection. May be NULL if the client
	// dropped — we still need to drain/reap the CGI in that case.
	Connection *conn = NULL;
	std::map<int, Connection *>::iterator connIt = _fdToConnMap.find(cgi.connFd);
	if (connIt != _fdToConnMap.end())
	{
		conn = connIt->second;
	}

	char buffer[CGI_BUFFER_SIZE];
	ssize_t bytes_read = read(cgiFd, buffer, sizeof(buffer));
	std::cout << "[WebServ] CGI bytes read " << bytes_read << std::endl;

	if (bytes_read > 0)
	{
		// Output is progress for the watchdog.
		cgi.startTime = time(NULL);

		if (conn == NULL)
		{
			// Client gone; just drop bytes until EOF or error.
			return;
		}

		size_t bufConsumed = 0;

		// --- Header phase: accumulate until we find \r\n\r\n / \n\n ---
		if (!cgi.headersParsed)
		{
			size_t headroom = MAX_CGI_HEADER_SIZE - cgi.headerBuf.size();
			size_t toCopy = std::min(static_cast<size_t>(bytes_read), headroom);
			cgi.headerBuf.append(buffer, toCopy);
			bufConsumed = toCopy;

			_tryParseCGIHeaders(cgi, conn);

			if (!cgi.headersParsed && cgi.headerBuf.size() >= MAX_CGI_HEADER_SIZE)
			{
				// Boundary not found within the cap — malformed CGI.
				// We can still recover with a clean 500 because no
				// bytes have gone to the client yet.
				std::cerr << "[WebServ] CGI header section exceeded "
						  << MAX_CGI_HEADER_SIZE << " bytes — aborting" << std::endl;
				int saveConnFd = cgi.connFd;
				_terminateCGIProcess(cgiFd);
				(*index)--;
				_sendCGIErrorResponse(saveConnFd);
				return;
			}
		}

		// --- Body phase: framed chunk straight into the client queue ---
		if (cgi.headersParsed && bufConsumed < static_cast<size_t>(bytes_read))
		{
			_appendBodyChunk(cgi, conn,
							 buffer + bufConsumed,
							 static_cast<size_t>(bytes_read) - bufConsumed);
		}

		_syncStreamPollFlags(cgi, conn);
		return;
	}

	if (bytes_read == 0)
	{
		// -------- EOF: CGI closed stdout --------
		int status = 0;
		waitpid(cgi.pid, &status, 0);
		cgi.pid = -1;
		if (conn)
			conn->setCgiPid(-1); // reaped here; disarm ~Connection's kill net

		bool exitedCleanly = WIFEXITED(status) && WEXITSTATUS(status) == 0;

		if (!cgi.headersParsed)
		{
			// Pre-header failure: we never sent anything → clean 500.
			if (WIFSIGNALED(status))
			{
				std::cout << "[WebServ] CGI killed by signal "
						  << WTERMSIG(status) << " before any headers" << std::endl;
			}
			else if (!exitedCleanly)
			{
				std::cout << "[WebServ] CGI exited "
						  << WEXITSTATUS(status) << " before any headers" << std::endl;
			}
			else
			{
				std::cout << "[WebServ] CGI exited cleanly but produced no headers" << std::endl;
			}

			int saveConnFd = cgi.connFd;

			// Tear down CGI state. _closeCGIStdin handles the stdin
			// pipe + cursor; close the stdout pipe explicitly.
			_closeCGIStdin(cgi, index);
			close(cgiFd);
			_cgiProcesses.erase(cgiIt);
			_pollFds[*index] = _pollFds.back();
			_pollFds.pop_back();
			(*index)--;

			_sendCGIErrorResponse(saveConnFd);
			return;
		}

		// -------- EOF after headers were sent --------
		if (conn)
		{
			if (exitedCleanly)
			{
				// Clean finish — emit chunked terminator if applicable.
				if (cgi.chunked && !cgi.headOnly)
				{
					conn->appendToStreamBuffer("0" DBL_CRLF, 5);
				}
			}
			else
			{
				// Post-header crash. Cannot send 500 because the
				// status line is already on the wire — best we can
				// do is flush whatever's buffered then close.
				if (WIFSIGNALED(status))
				{
					std::cout << "[WebServ] CGI killed by signal "
							  << WTERMSIG(status) << " mid-stream" << std::endl;
				}
				else
				{
					std::cout << "[WebServ] CGI exited "
							  << WEXITSTATUS(status) << " mid-stream" << std::endl;
				}
				conn->markStreamAborted();
			}
			conn->markStreamFinished();

			// Make sure POLLOUT is on so the buffered tail flushes.
			_setPollEvents(cgi.connFd, POLLOUT, POLLIN);
		}

		// Tear down CGI bookkeeping. Connection stays alive until
		// its stream buffer drains.
		_closeCGIStdin(cgi, index);
		close(cgiFd);
		_cgiProcesses.erase(cgiIt);
		_pollFds[*index] = _pollFds.back();
		_pollFds.pop_back();
		(*index)--;
		return;
	}

	// -------- read() < 0: pipe error --------
	if (cgi.headersParsed && conn)
	{
		// Already streaming — abort the connection.
		std::cerr << "[WebServ] CGI pipe error mid-stream on FD " << cgiFd << std::endl;
		conn->markStreamAborted();
		conn->markStreamFinished();
		_setPollEvents(cgi.connFd, POLLOUT, POLLIN);

		kill(cgi.pid, SIGKILL);
		waitpid(cgi.pid, NULL, 0);
		conn->setCgiPid(-1); // reaped here; disarm ~Connection's kill net
		_closeCGIStdin(cgi, index);
		close(cgiFd);
		_cgiProcesses.erase(cgiIt);
		_pollFds[*index] = _pollFds.back();
		_pollFds.pop_back();
		(*index)--;
		return;
	}

	// Pre-header error: clean 500.
	int saveConnFd = cgi.connFd;
	_terminateCGIProcess(cgiFd);
	(*index)--;
	_sendCGIErrorResponse(saveConnFd);
}

// --------------------------------------------------------------------
// CGI streaming helpers
// --------------------------------------------------------------------

bool WebServ::_tryParseCGIHeaders(CGIProcess &cgi, Connection *conn)
{
	if (cgi.headersParsed)
		return true;

	// Locate the CGI header/body boundary inside the accumulator.
	size_t boundaryPos = cgi.headerBuf.find(DBL_CRLF);
	size_t delimLen = 4;
	if (boundaryPos == std::string::npos)
	{
		size_t alt = cgi.headerBuf.find(LF LF);
		if (alt != std::string::npos)
		{
			boundaryPos = alt;
			delimLen = 2;
		}
		else
		{
			return false; // need more bytes
		}
	}

	std::string headersPart = cgi.headerBuf.substr(0, boundaryPos);

	HTTP_Response &response = conn->getResponse();
	response.reset();
	response.setStatus(HTTP_Status::OK);
	response.setCGIGenerated(true);

	std::vector<std::string> lines = Utils::split(headersPart, '\n');
	for (size_t i = 0; i < lines.size(); ++i)
	{
		std::string line = Utils::trim(lines[i]);
		if (line.empty())
			continue;

		// NPH (Non-Parsed Header): first line may be "HTTP/1.x <code> ..."
		if (i == 0 && line.length() > 5 && line.substr(0, 5) == "HTTP/")
		{
			std::vector<std::string> parts = Utils::split(line, ' ');
			if (parts.size() >= 2)
			{
				response.setStatus(HTTP_Status::fromCode(Utils::toInt(parts[1])));
			}
			continue;
		}

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string name = Utils::trim(line.substr(0, colon));
		std::string value = Utils::trim(line.substr(colon + 1));
		std::string low = Utils::toLowerCase(name);

		if (low == "status")
		{
			response.setStatus(HTTP_Status::fromCode(Utils::toInt(value)));
			continue;
		}
		// We own framing. Strip anything the CGI tried to set —
		// mixing CGI-supplied Content-Length / Transfer-Encoding with
		// our chunked framing would lie to the client.
		if (low == "content-length" || low == "transfer-encoding")
		{
			continue;
		}
		response.getHeaders()[name] = value;
	}

	// Decide downstream framing. HTTP/1.1 → chunked; otherwise close.
	HTTP_Request &request = conn->getRequest();
	bool isHttp11 = (request.getVersion() == HTTP_Version::v1_1);
	cgi.chunked = isHttp11;
	cgi.headOnly = (request.getMethod() == HTTP_Method::HEAD);

	if (cgi.chunked)
	{
		response.getHeaders()[HTTP_FieldName::TRANSFER_ENCODING] = "chunked";
	}
	else
	{
		// HTTP/1.0: no chunked; must close so client sees the boundary.
		response.getHeaders()[HTTP_FieldName::CONNECTION] = "close";
	}

	// Push status line + headers + CRLF into the connection's queue.
	std::string prefix;
	response.serializeHeadersInto(prefix);
	conn->appendToStreamBuffer(prefix);

	conn->setState(Connection::STREAMING_CGI);
	_setPollEvents(cgi.connFd, POLLOUT, POLLIN);

	cgi.headersParsed = true;

	// Bytes that came in past the boundary in the header buffer ARE
	// the start of the body — frame them as the first chunk.
	size_t bodyStart = boundaryPos + delimLen;
	if (bodyStart < cgi.headerBuf.size())
	{
		_appendBodyChunk(cgi, conn,
						 cgi.headerBuf.data() + bodyStart,
						 cgi.headerBuf.size() - bodyStart);
	}
	std::string().swap(cgi.headerBuf); // free header accumulator
	return true;
}

void WebServ::_appendBodyChunk(CGIProcess &cgi, Connection *conn,
							   const char *data, size_t n)
{
	if (n == 0 || conn == NULL)
		return;
	if (cgi.headOnly)
		return; // HEAD: discard CGI body silently

	if (cgi.chunked)
	{
		// <hex>\r\n<data>\r\n
		char hexBuf[32];
		int hexLen = std::sprintf(hexBuf, "%lx" CRLF, static_cast<unsigned long>(n));
		if (hexLen > 0)
		{
			conn->appendToStreamBuffer(hexBuf, static_cast<size_t>(hexLen));
		}
		conn->appendToStreamBuffer(data, n);
		conn->appendToStreamBuffer(CRLF, 2);
	}
	else
	{
		conn->appendToStreamBuffer(data, n);
	}
}

void WebServ::_syncStreamPollFlags(CGIProcess &cgi, Connection *conn)
{
	if (conn == NULL)
		return;

	size_t bufSize = conn->streamBufferSize();

	// POLLOUT on client whenever we have something queued. (Cleared
	// again in _sendResponse once the buffer drains.)
	if (bufSize > 0)
	{
		_setPollEvents(cgi.connFd, POLLOUT, 0);
	}

	// Back-pressure: pause/resume CGI reads based on buffer size.
	if (bufSize >= CGI_FORWARD_HIGH_WATER && !cgi.pollinPaused)
	{
		_setPollEvents(cgi.stdoutFd, 0, POLLIN);
		cgi.pollinPaused = true;
	}
	else if (bufSize < CGI_FORWARD_LOW_WATER && cgi.pollinPaused)
	{
		_setPollEvents(cgi.stdoutFd, POLLIN, 0);
		cgi.pollinPaused = false;
	}
}

void WebServ::_sendCGIErrorResponse(int connFd)
{
	std::map<int, Connection *>::iterator it = _fdToConnMap.find(connFd);
	if (it == _fdToConnMap.end())
		return;
	Connection *conn = it->second;

	conn->getResponse().setStatus(HTTP_Status::INTERNAL_SERVER_ERROR);
	conn->setState(Connection::ERROR);
	conn->prepareResponseDirect();

	_setPollEvents(connFd, POLLOUT, POLLIN);
}

size_t WebServ::_findPollIndex(int fd) const
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
			return i;
	}
	return static_cast<size_t>(-1);
}

void WebServ::_setPollEvents(int fd, short enable, short disable)
{
	_updateEvent(_findPollIndex(fd), enable, disable);
}

void WebServ::_checkCGITimeouts()
{
	std::vector<int> to_cleanup;
	time_t now = time(NULL);

	for (std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.begin();
		 cgiIt != _cgiProcesses.end(); ++cgiIt)
	{
		if (now - cgiIt->second.startTime > CGI_TIMEOUT)
		{
			to_cleanup.push_back(cgiIt->first);
		}
	}

	for (size_t i = 0; i < to_cleanup.size(); ++i)
	{
		std::cout << "[WebServ] CGI timeout: killing process on FD " << to_cleanup[i] << std::endl;
		// _cleanupCGI fully tears down the CGI, including removing its
		// stdout FD from _pollFds, so no extra poll-array cleanup here.
		_cleanupCGI(to_cleanup[i]);
	}
}

void WebServ::_cleanupCGI(int cgiFd)
{
	std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.find(cgiFd);
	if (cgiIt == _cgiProcesses.end())
	{
		return;
	}

	// Save the client FD and headers-sent state before we destroy
	// the CGI bookkeeping — _terminateCGIProcess will erase it.
	int connFd = cgiIt->second.connFd;
	bool headersParsed = cgiIt->second.headersParsed;

	// 1. Silently kill the script and clean up its pipes
	_terminateCGIProcess(cgiFd);

	// 2. React according to what (if anything) is already on the wire.
	std::map<int, Connection *>::iterator connIt = _fdToConnMap.find(connFd);
	if (connIt == _fdToConnMap.end())
		return;
	Connection *conn = connIt->second;

	if (!headersParsed)
	{
		// Nothing forwarded yet — clean 500 is still possible.
		_sendCGIErrorResponse(connFd);
	}
	else
	{
		// Status line + some body already on the wire. Can't change
		// status now — flush what's buffered then close.
		conn->markStreamAborted();
		conn->markStreamFinished();
		_setPollEvents(connFd, POLLOUT, POLLIN);
	}
}

// A silent killer: Just kills the process, closes both pipes, and removes
// from poll. Does NOT touch the Connection object or send HTTP errors.
void WebServ::_terminateCGIProcess(int cgiFd)
{
	std::map<int, CGIProcess>::iterator it = _cgiProcesses.find(cgiFd);
	if (it == _cgiProcesses.end())
		return;

	// 1. Close stdin first: drops it from the poll set and makes any
	// in-flight child write() see EPIPE instead of blocking. NULL cursor —
	// also called outside the poll loop.
	_closeCGIStdin(it->second, NULL);

	// 2. Kill and reap. Blocking waitpid after SIGKILL — the child is
	// reapable almost instantly; WNOHANG could leave a zombie.
	kill(it->second.pid, SIGKILL);
	waitpid(it->second.pid, NULL, 0);

	// Reaped here — clear the owning connection's pid so ~Connection
	// doesn't kill/waitpid a stale (possibly recycled) PID.
	std::map<int, Connection *>::iterator c = _fdToConnMap.find(it->second.connFd);
	if (c != _fdToConnMap.end())
		c->second->setCgiPid(-1);

	// 3. Close the stdout pipe
	close(cgiFd);

	// 4. Remove stdout FD from poll array (O(1) efficiency)
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == cgiFd)
		{
			_pollFds[i] = _pollFds.back();
			_pollFds.pop_back();
			break;
		}
	}

	// 5. Remove from CGI map
	_cgiProcesses.erase(it);
}
