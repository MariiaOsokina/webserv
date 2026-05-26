/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/11 12:49:10 by mosokina          #+#    #+#             */
/*   Updated: 2026/05/26 19:24:46 by mosokina         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

Connection::Connection(int fd, Listener *listener): _state(READING_HEADERS),
												_connectFd(fd),
												_listener(listener),
												_expectedBodySize(0),
												_isChunked(false),
												_responseBuilder(listener->getConfig())
{
	_lastActive = std::time(NULL);
	_cgi_pid = -1; // -1 means no CGI process is currently running
}

Connection::~Connection()
{
    // 1. Close the client socket
    if (_connectFd != -1) {
        close(_connectFd);
        _connectFd = -1;
    }

    // 2. Clean up any rogue CGI processes
    if (_cgi_pid > 0) {
        // Send a kill signal to force the child to stop immediately
        kill(_cgi_pid, SIGKILL); 
        
        // Tell the OS to reap the zombie and delete its record.
        waitpid(_cgi_pid, NULL, 0); 
    }
}

void Connection::resetForNextRequest()
{
	_request.reset();
	_response.reset();
	_responseBuilder.reset();
	_bytesSent = 0;
	_state = READING_HEADERS;
	_chunkedAccumulator.clear();
	_isChunked = false;
	_expectedBodySize = 0;
	_cgi_pid = -1;
	// Log for debugging:
	if (!_rawRequest.empty()) {
		std::cout << "[WebServ] Pipelined data remaining in buffer: " 
				  << _rawRequest.size() << " bytes." << std::endl;
	}
}

void Connection::resetTimeout()
{
	_lastActive = std::time(NULL);
}

bool Connection::isTimedOut(time_t now, int limit) const
{
	return (std::difftime(now, _lastActive) >= limit);
}
HTTP::Request &Connection::getRequest()
{
	return (_request);
}

HTTP::Response &Connection::getResponse()
{
	return (_response);
}

Listener *Connection::getServer()
{
	return (_listener);
}

int Connection::getState() const
{
	return _state;
}

void Connection::setState(ConnectionState state)
{
    _state = state;
}

void Connection::setCgiPid(pid_t pid)
{
    _cgi_pid = pid;
}

void Connection::handleRead(const char *buffer, ssize_t bytesRead)
{
	// Safety check: Don't let _rawRequest grow indefinitely while looking for headers
	if (buffer != NULL && bytesRead > 0)
	{
		if (_state == READING_HEADERS && (_rawRequest.size() + bytesRead > MAX_HEADER_SIZE))
		{
			_state = ERROR;
			_request.setParseStatus(HTTP_Request::REQUEST_HEADER_FIELDS_TOO_LARGE);
			return;
		}
		_rawRequest.append(buffer, bytesRead);        
	}
	if (_state == READING_HEADERS)
	{
		size_t pos = _rawRequest.find(DBL_CRLF);
		if (pos != std::string::npos) {
			std::string headerString = _rawRequest.substr(0, pos + 4);
			
			if (_request.parseHeaders(headerString.c_str(), headerString.size()) == FAILURE) {
				_state = ERROR;
				return;
			}

			// Remove headers from buffer; only the start of the body remains
			_rawRequest.erase(0, pos + 4);
			_setupBodyReading();
		}
	}

	if (_state == READING_BODY) {
		if (_isChunked)
			_handleChunkedBody();
		else
			_handleStandardBody();
	}
	std::cout << "[DEBUG] Bytes in Request object: " << _request.getBody().length() << std::endl;
}

bool Connection::handleWrite() //bool is finised
{
	if (_rawResponse.empty())
		return true; // //MO: check case, Nothing to send

	const char* dataPtr = _rawResponse.c_str() + _bytesSent;
	size_t remaining = _rawResponse.size() - _bytesSent;

	ssize_t sent = send(_connectFd, dataPtr, remaining, 0);

	// RULE: No errno check. If sent is -1 (error) or 0 (failed to move any bytes/closed)
    if (sent <= 0)
    {
        _state = ERROR;
        return true; 
    }

	_bytesSent += sent;
	this->resetTimeout(); //MO: find proper place for this line
	if (_bytesSent >= _rawResponse.size()) {
		return true;
	}
	return false;
}

bool Connection::shouldClose() const
{
	if (_state == ERROR) return true;
	// MO: HANDLE OTHER ERRORS
	if (_request.getParseStatus() == HTTP_Request::BAD_REQUEST)
		return true;

	const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = _request.getHeaders();
	std::string version = _request.getVersion();

	// Find the 'Connection' header
	std::string connHeader = "";
	std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator it = headers.find("Connection");
		if (it != headers.end()) {
			connHeader = it->second; // C++98 safe!
			for (size_t i = 0; i < connHeader.length(); ++i)
				connHeader[i] = std::tolower(connHeader[i]);
		}
	// 1. HTTP/1.1 Logic: Persistent by default
	if (version == HTTP_Version::v1_1) {
		return (connHeader == "close");
	}

	// 2. HTTP/1.0 Logic: Close by default
	if (version == HTTP_Version::v1_0) {
		return (connHeader != "keep-alive");
	}

	return true; // Default to safety
}

void Connection::prepareResponse()
{
	_responseBuilder.build(_response, _request); //MO: CGI starts here!!
	// Build wire bytes directly into _rawResponse and transfer body
	// ownership in O(1). Avoids the ostringstream double-copy that was
	// peaking at ~3× the body and OOM-killing the server under
	// concurrent 100 MB uploads.
	_response.serializeInto(_rawResponse);
	_bytesSent = 0;
}

void Connection::forceTimeoutError()
{
	_state = ERROR;
	_request.setParseStatus(HTTP_Request::REQUEST_TIMEOUT);
}

void Connection::_setupBodyReading()
{
    const std::map<std::string, std::string, CaseInsensitiveCompare>& h = _request.getHeaders();
    std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator itTE = h.find("Transfer-Encoding");
    std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator itCL = h.find("Content-Length");
    
    // 1. RFC 9112: Priority & Smuggling Check
    // If both are present, we must reject it.
    if (itTE != h.end() && itCL != h.end())
    {
        std::cout << "[Security] Smuggling attempt: both CL and TE present." << std::endl;
        _request.setParseStatus(HTTP_Request::BAD_REQUEST);
        _state = ERROR;
        return;
    }

    // 2. Handle Transfer-Encoding
    if (itTE != h.end())
    {
        // We ONLY support 'chunked'. (Using strcasecmp because HTTP values are case-insensitive)
        if (strcasecmp(itTE->second.c_str(), "chunked") == 0)
        {
            _isChunked = true;
            _state = READING_BODY;
        }
        else
        {
            std::cout << "[WebServ] Unsupported TE: " << itTE->second << std::endl;
            _request.setParseStatus(HTTP_Request::BAD_REQUEST); //or 501 (Not Implemented)
            _state = ERROR;
        }
        return;
    }
    // 3. Handle Content-Length
    if (itCL != h.end()) 
    {
        _expectedBodySize = std::strtoul(itCL->second.c_str(), NULL, 10);        
        size_t maxBodySize = _listener->getConfig().client_max_body_size;
		//
		// AI: the below commented section is a demo of how to
		// read client_max_body_size from a location at any time,
		// after the request line has been parsed
		//
		//ssize_t max_body_size = HTTP::ResponseBuilder::getClientMaxBodySize(_listener->getConfig(), _request, _response);
		//if (max_body_size == -1)
		//{
		//	// HTTP::ResponseBuilder::getClientMaxBodySize will set _response accordingly
		//	// send _response.serialise()
		//}
		//size_t maxBodySize = static_cast<size_t>(max_body_size);
        if (maxBodySize > 0 && _expectedBodySize > maxBodySize) {
            std::cout << "[WebServ] Payload too large (Content-Length): " << _expectedBodySize << std::endl;
            _request.setParseStatus(HTTP_Request::CONTENT_TOO_LARGE);
            _state = ERROR;
            return;
        }

        _state = (_expectedBodySize > 0) ? READING_BODY : REQUEST_READY;
    }
    else 
        _state = REQUEST_READY; // No TE and no CL means no body.
}

bool Connection::hasBufferedData() const
{
	return !_rawRequest.empty();
}

std::string Connection::getRawRequest() const
{
	return _rawRequest;
}

std::string Connection::getRawResponse() const
{
	return _rawResponse;
}

void Connection::_handleStandardBody()
{
    size_t available = _rawRequest.size();
    size_t needed = _expectedBodySize - _request.getBody().length();
    size_t toMove = std::min(available, needed);

    if (toMove > 0) {
        // Move data from the raw buffer to the Request object
		//_request.setBody(_rawRequest.substr(0, toMove), toMove);
        _request.appendToBody(_rawRequest.substr(0, toMove), toMove, _request.getBody().length() >= _expectedBodySize); // AI: updated with _request.getBody().length() >= _expectedBodySize
        _rawRequest.erase(0, toMove); // Remove it from the socket buffer
        
        std::cout << "[DEBUG] Moved " << toMove << " bytes to Request body." << std::endl;
    }
    // CRITICAL: Only set to REQUEST_READY if we actually have all the bytes!
    if (_request.getBody().length() >= _expectedBodySize) {
        _state = REQUEST_READY;
        std::cout << "[DEBUG] Standard body fully read. Total: " << _request.getBody().length() << std::endl;
    }
}

void Connection::_handleChunkedBody() {
	while (true)
	{
		size_t pos = _rawRequest.find(CRLF);
		if (pos == std::string::npos) return; // Wait for more data

		// 1. Extract hex size, ignoring any chunk extensions (e.g., "5;ext=val")
		std::string hexLine = _rawRequest.substr(0, pos);
		size_t semiPos = hexLine.find(';');
		std::string hexStr = (semiPos != std::string::npos) ? hexLine.substr(0, semiPos) : hexLine;
		
		// Validation check
		if (!_isValidHex(hexStr)) {
			std::cout << "[WebServ] Invalid chunk size format" << std::endl;
			_request.setParseStatus(HTTP_Request::BAD_REQUEST);
			_state = ERROR;
			return;
		}

		size_t chunkSize = std::strtoul(hexStr.c_str(), NULL, 16);
		
		// 2. Terminal Chunk Handling (0\r\n\r\n)
		if (chunkSize == 0) {
			// Search for the end of the entire chunked message (handles trailing headers)
			size_t endPos = _rawRequest.find(DBL_CRLF, pos);
			if (endPos == std::string::npos) return; // Wait for the final CRLF

			//_request.setBody(_chunkedAccumulator.c_str(), _chunkedAccumulator.size()); // AI: changed setBody(...) to appendToBody(...)
			_request.appendToBody(_chunkedAccumulator.c_str(), _chunkedAccumulator.size(), true);
			_state = REQUEST_READY;
			_rawRequest.erase(0, endPos + 4); 
			return;
		}

		// 3. Overflow and Limit Check for Payload Size
		size_t maxBodySize = _listener->getConfig().client_max_body_size;
		if (maxBodySize > 0 && (chunkSize > maxBodySize || _chunkedAccumulator.size() + chunkSize > maxBodySize))
		{
			std::cout << "[WebServ] Payload too large (Chunked stream exceeded limit)" << std::endl;
			_request.setParseStatus(HTTP_Request::CONTENT_TOO_LARGE);
			_state = ERROR;
			return;
		}

		// 4. Safe Bounds Check (Prevents integer overflow)
		if (_rawRequest.size() - (pos + 2) < chunkSize + 2) return;

		// 5. _handleChunkedBody
		if (_rawRequest.substr(pos + 2 + chunkSize, 2) != "\r\n") {
			std::cout << "[WebServ] Malformed chunk: missing trailing CRLF" << std::endl;
			_request.setParseStatus(HTTP_Request::BAD_REQUEST);
			_state = ERROR;
			return;
		}

		// 6. Append data and erase processed chunk
		_chunkedAccumulator.append(_rawRequest.substr(pos + 2, chunkSize));
		_rawRequest.erase(0, pos + 2 + chunkSize + 2); // Move to next chunk
	}
}

bool Connection::_isValidHex(const std::string& s) const
{
	if (s.empty()) return false;
	for (size_t i = 0; i < s.size(); ++i) {
		if (!std::isxdigit(static_cast<unsigned char>(s[i]))) {
			return false;
		}
	}
	return true;
}

bool Connection::isChunked() const
{
	return (_isChunked);
}