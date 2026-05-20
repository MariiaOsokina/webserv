/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServ.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:03:57 by aistok            #+#    #+#             */
/*   Updated: 2026/05/20 13:00:37 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "WebServ.hpp"
#include "ErrorPages.hpp"
#include "CGI.hpp"

#include <sys/wait.h> // For waitpid and WNOHANG
#include <signal.h>   // For kill and SIGKILL

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
		Listener *newListener = NULL;
		try
		{
			newListener = new Listener(configs[i]);
			newListener->initSocket(); // Creates the socket, bind, listen
			int listenFd = newListener->getListenFd();

			_listeners.push_back(newListener);
			_addNewFdtoPool(listenFd, POLLIN);
			_fdToListenerMap[listenFd] = newListener;
			std::cout << "[WebServ] New Listener setted up on FD: " << listenFd << std::endl; // log
		}
		catch (const std::exception &e)
		{
			std::cerr << "Failed to setup listener " << configs[i].host << ":" << configs[i].ports[0] << ": " << e.what() << std::endl;
			if (newListener)
				delete newListener;
		}
	}
}

/*The Rule:
1 - CGI PIPES: Priority check for internal pipes.
Handle CGI output (POLLIN) or process termination (POLLHUP). Use read() to capture the script's response.

2 - READ (POLLIN): Always check this first. Call recv().
If recv() returns > 0, we got data.
If recv() returns 0, this is signal that the client closed the connection.

3 - WRITE (POLLOUT): Check this if you have data queued to send.

4 - ERRORS (POLLERR, POLLNVAL, POLLHUP): Use these only as a "fallback" or for fatal system errors.*/

void WebServ::run(void)
{
	time_t lastTimeoutCheck = std::time(NULL);

    while (g_server_running)
    {
        time_t now = std::time(NULL);
        // Only run the heavy O(N^2) check once per second
        if (now - lastTimeoutCheck >= 1) {
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
            if (_isCGISocket(_pollFds[i].fd)) {
                if (_pollFds[i].revents & (POLLIN | POLLHUP)) {
                    _handleCGIOutput(&i);
                } else if (_pollFds[i].revents & (POLLERR | POLLNVAL)) {
                    std::cout << "[WebServ] CGI fd: " << _pollFds[i].fd << " removed due to error" << std::endl;
                    _cleanupCGI(_pollFds[i].fd);
                    _closeCGIPipe(i); //MO: addded
                    i--;
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

    // 1. CHECK FOR ORPHANED CGIs BEFORE DESTROYING CONNECTION
    std::map<int, CGIProcess>::iterator it = _cgiProcesses.begin();
    while (it != _cgiProcesses.end()) {
        if (it->second.connFd == fd) {
            std::cout << "[WebServ] Client dropped. Killing orphaned CGI PID: " << it->second.pid << std::endl;
            
            // Call our new helper using the CGI's pipe FD
            _terminateCGIProcess(it->second.stdoutFd);
            
            // We MUST break here because the iterator 'it' was just destroyed by the helper!
            break; 
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
    _pollFds[index] = _pollFds.back();
    _pollFds.pop_back();

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
        
        if (bytesRead > 0) {
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
			//If poll() flags POLLIN, the kernel is promising you there is data to read (or the connection is closed).
			std::cerr << "[WebServ] Fatal recv error on FD: " << fd << ". Closing." << std::endl;

			_closeConnection(*index);
            (*index)--;
            return ;
        }
    }
	
	// 3. Final State Check
    if (conn->getState() == Connection::REQUEST_READY || conn->getState() == Connection::ERROR)
	{
        std::cout << "[WebServ] Request Complete on FD " << fd << std::endl;
        // This routes the request and sets the CGI flags if applicable
        conn->prepareResponse();
        // --- NEW CGI EXECUTION TRIGGER START ---
        // If no errors occurred during parsing/routing AND it's flagged as a CGI request
        if (conn->getState() != Connection::ERROR && conn->getResponse().isCGIGenerated())
		{
            // Extract the paths saved by the ResponseBuilder
            std::string cgiExecPath = conn->getResponse().getCgiPath();
            std::string scriptPath = conn->getResponse().getScriptPath();

            std::cout << "[WebServ] Routing to CGI. Script: " << scriptPath << std::endl;
            
            // Start the non-blocking execution
            _executeCGI(fd, cgiExecPath, scriptPath);
            
            // CRITICAL: Return immediately! 
            // Do NOT switch to POLLOUT yet. The CGI state machine will handle that 
            // when the script finishes.
            return; 
        }
        // --- NEW CGI EXECUTION TRIGGER END ---

        // If it is NOT a CGI request (or an error occurred), proceed normally to send static data
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
		return ;
	}
	Connection *conn = it->second;
	conn->resetTimeout();

	// NEW: Loop to flush all ready pipelined requests sequentially
	int requestsProcessed = 0;
	while (requestsProcessed < 10) // Limit to 10 requests per poll trigger
	{
		bool finished = conn->handleWrite();
		//std::cout << "[DEBUG] Sending Response Body: " << conn->getRawResponse().substr(0, 15) << "..." << std::endl;	
        std::cout << "[DEBUG] Sending Response Body: " << Utils::substrUpTo(conn->getRawResponse(), CRLF) << std::endl; // AI: added for degubbing purposes, to see response header clearly
		if (finished)
		{
			requestsProcessed++;
			if (conn->shouldClose()) {
				std::cout << "[WebServ] Closing connection on FD " << fd << std::endl;
				_closeConnection(*index);
				(*index)--;
				return ;
			}
			else //keep open (Keep-Alive)
			{
				std::cout << "[WebServ] Keeping connection alive on FD " << fd << std::endl;
				conn->resetForNextRequest();
				_updateEvent(*index, POLLIN, POLLOUT);

				// CRITICAL FOR PIPELINING:
				if (conn->hasBufferedData()) {
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
				return ; 
			}
		}
		else // send() returned EAGAIN/EWOULDBLOCK. Need to wait for next POLLOUT event.			
			return ;
	}
}

void WebServ::_checkConnTimeouts()
{
	time_t now = std::time(NULL);
	std::map<int, Connection*>::iterator it = _fdToConnMap.begin();

	while (it != _fdToConnMap.end())
	{
		int fd = it->first;
		Connection *conn = it->second;
		std::map<int, Connection*>::iterator next = it;
		++next;

		if (conn->isTimedOut(now, CONNECTION_TIMEOUT))
		{
			// 1. Find the index in _pollFds once
			int pollIdx = -1;
			for (size_t i = 0; i < _pollFds.size(); ++i) {
				if (_pollFds[i].fd == fd) {
					pollIdx = i;
					break;
				}
			}

			if (pollIdx == -1) { 
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
			//TO-DO: if the state is WAITING_FOR_CGI - send a 504 Gateway Timeout

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

bool WebServ::_isCGISocket(int fd) const {
    return (_cgiProcesses.find(fd) != _cgiProcesses.end());
}

bool WebServ::_executeCGI(int connFd, const std::string& cgiPath, const std::string& scriptPath) {
    Connection *conn = _fdToConnMap[connFd];

    CGI cgi(cgiPath, scriptPath, conn->getRequest());

    //Start non-blocking CGI execution.
    std::pair<pid_t, int> res = cgi.executeNonBlocking();

    if (res.first == -1) {

        std::cerr << "[WebServ] CGI fork failed for FD " << connFd << std::endl;
        conn->getResponse().setStatus(HTTP_Status::INTERNAL_SERVER_ERROR);
        conn->prepareResponse();
		conn->setState(Connection::ERROR); //MO: If fork fails, the request is dead. Set to ERROR
        
        // Find the client/connection in poll and set them to WRITE immediately
        for (size_t i = 0; i < _pollFds.size(); ++i) {
            if (_pollFds[i].fd == connFd) {
                _updateEvent(i, POLLOUT, POLLIN);
                break;
            }
        }
        return false; //Fork failed.
    }
    conn->setCgiPid(res.first);
    //We need to store CGI process info
    CGIProcess process;
    process.pid = res.first;
    process.stdoutFd = res.second;
    process.connFd = connFd;
    process.startTime = time(NULL);

    _cgiProcesses[res.second] = process;
    
	// MO: Set to WAITING_FOR_CGI here, because the fork succeeded!
    conn->setState(Connection::WAITING_FOR_CGI);
    //We set client/connection state to waiting state i.e: not reading or writing.
    _addNewFdtoPool(res.second, POLLIN);

    return true;
}

void WebServ::_handleCGIOutput(size_t *index) {
    int cgiFd = _pollFds[*index].fd;

    std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.find(cgiFd);
    if (cgiIt == _cgiProcesses.end()) {
        return;
    }
    
    CGIProcess& cgi = cgiIt->second;
    
    char buffer[CGI_BUFFER_SIZE];
    ssize_t bytes_read = read(cgiFd, buffer, sizeof(buffer));
    std::cout << "[WebServ] CGI bytes read " <<bytes_read << std::endl;
    if (bytes_read > 0) {
        cgi.output.append(buffer, bytes_read);
    } else if (bytes_read == 0) {
        // ==========================================
        // EOF - CGI FINISHED SUCCESSFULLY
        // ==========================================
        
        // 1. Wait for child process to prevent zombies
        int status;
        waitpid(cgi.pid, &status, WNOHANG);
        
        // 2. Find client/connection and send response
        std::map<int, Connection*>::iterator it = _fdToConnMap.find(cgi.connFd);
        if (it != _fdToConnMap.end()) {
            Connection* conn = it->second;
            
            // Check if CGI produced any output
            if (cgi.output.empty()) {
                std::cout << "[WebServ] CGI produced no output - likely execution failed" << std::endl;
                conn->getResponse().setStatus(HTTP_Status::INTERNAL_SERVER_ERROR);
                conn->setState(Connection::ERROR);
            } else {                
                // Parse CGI output into response
                conn->getResponse() = CGI::parseCGIOutput(cgi.output);
                conn->setState(Connection::REQUEST_READY);
            }
            
            conn->prepareResponse();

            // Find client/connection in poll array and switch them to POLLOUT
            for (size_t i = 0; i < _pollFds.size(); ++i) {
                if (_pollFds[i].fd == cgi.connFd) {
                    _updateEvent(i, POLLOUT, POLLIN);
                    break;
                }
            }
        }
        
        // 3. Clean up pipes and maps manually (Since it was successful, we don't kill it)
        close(cgiFd);
        _fdToConnMap.erase(cgiFd); // MO: Fixed map leak!
        _cgiProcesses.erase(cgiIt);
        
        // 4. Remove from poll array (O(1) efficiency)
        _pollFds[*index] = _pollFds.back();
        _pollFds.pop_back();
        (*index)--; // Adjust index because vector shrank and swapped a new FD here
        
    } else {
        // ==========================================
        // ERROR - READ FAILED
        // ==========================================
        
        // This helper handles everything: kills script, closes pipe, 
        // cleans all maps, removes from _pollFds, and sends 500 error!
        _cleanupCGI(cgiFd); 
        
        // We MUST still decrement the index because _cleanupCGI used the 
        // swap/pop method inside _terminateCGIProcess, meaning the current 
        // index now holds a completely different FD that needs to be checked.
        (*index)--;
    }
}

void WebServ::_checkCGITimeouts() {
    std::vector<int> to_cleanup;
    time_t now = time(NULL);

    for (std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.begin();
         cgiIt != _cgiProcesses.end(); ++cgiIt) {
        if (now - cgiIt->second.startTime > CGI_TIMEOUT) {
            to_cleanup.push_back(cgiIt->first);
        }
    }

    for (size_t i = 0; i < to_cleanup.size(); ++i) {
        std::cout << "[WebServ] CGI timeout: killing process on FD " << to_cleanup[i] << std::endl;
        _cleanupCGI(to_cleanup[i]);
        
        // Find the pipe in poll array and remove it safely
        for (size_t j = 0; j < _pollFds.size(); ++j) {
            if (_pollFds[j].fd == to_cleanup[i]) {
                _closeCGIPipe(j);
                j--;
                break;
            }
        }
    }
}

void WebServ::_cleanupCGI(int cgiFd) {
    std::map<int, CGIProcess>::iterator cgiIt = _cgiProcesses.find(cgiFd);
    if (cgiIt == _cgiProcesses.end()) {
        return;
    }

    // Save the client FD before we destroy the CGI process!
    int connFd = cgiIt->second.connFd; 

    // 1. Silently kill the script and clean up its pipes
    _terminateCGIProcess(cgiFd);

    // 2. Send the 500 Internal Server Error to the client
    std::map<int, Connection*>::iterator connIt = _fdToConnMap.find(connFd);
    if (connIt != _fdToConnMap.end()) {
        Connection* conn = connIt->second;
        conn->getResponse().setStatus(HTTP_Status::INTERNAL_SERVER_ERROR);
        conn->prepareResponse();
        
        conn->setState(Connection::ERROR);
        
        // Find client/connection and set to write
        for (size_t i = 0; i < _pollFds.size(); ++i) {
            if (_pollFds[i].fd == connFd) {
                _updateEvent(i, POLLOUT, POLLIN);
                break;
            }
        }
    }
}

// Custom closer for CGI pipes, so it doesn't accidentally trigger your Connection closer
void WebServ::_closeCGIPipe(size_t index) {
    _pollFds[index] = _pollFds.back();
    _pollFds.pop_back();
}

// A silent killer: Just kills the process, closes the pipe, and removes from poll.
// Does NOT touch the Connection object or send HTTP errors.
void WebServ::_terminateCGIProcess(int cgiFd) {
    std::map<int, CGIProcess>::iterator it = _cgiProcesses.find(cgiFd);
    if (it == _cgiProcesses.end()) return;

    // 1. Kill process and prevent zombies
    kill(it->second.pid, SIGKILL);
    waitpid(it->second.pid, NULL, WNOHANG);
    
    // 2. Close the pipe
    close(cgiFd);
    
    // 3. Safely remove from connection map
    _fdToConnMap.erase(cgiFd);
    
    // 4. Remove from poll array (O(1) efficiency)
    for (size_t i = 0; i < _pollFds.size(); ++i) {
        if (_pollFds[i].fd == cgiFd) {
            _pollFds[i] = _pollFds.back();
            _pollFds.pop_back();
            break;
        }
    }
    
    // 5. Remove from CGI map
    _cgiProcesses.erase(it);
}
