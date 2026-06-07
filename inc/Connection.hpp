/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/11 12:49:22 by mosokina          #+#    #+#             */
/*   Updated: 2026/06/06 20:48:19 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "HTTP.hpp"
#include "HTTP_Request.hpp"
#include "HTTP_Response.hpp"
#include "HTTP_ResponseBuilder.hpp"
#include "Listener.hpp"

#include <unistd.h>	  // close
#include <signal.h>	  // For SIGKILL
#include <sys/wait.h> // For waitpid
#include <sys/types.h>

#include <iostream>
#include <string>
#include <ctime>
#include <cstdlib>

class Listener;

class Connection
{
public:
	Connection(int fd, Listener *server); // check later -  const for Listener)
	~Connection();

	enum ConnectionState
	{
		READING_HEADERS,
		READING_BODY,
		REQUEST_READY,
		WAITING_FOR_CGI,
		STREAMING_CGI,
		ERROR
	};

	void resetTimeout();
	bool isTimedOut(time_t now, int limit) const;

	HTTP_Request &getRequest();
	HTTP_Response &getResponse();
	Listener *getServer();
	int getState() const;
	void setState(ConnectionState state); // <MO: new for CGI
	std::string getRawRequest() const;
	std::string getRawResponse() const;
	bool hasBufferedData() const;

	void handleRead(const char *buffer, ssize_t bytesRead);
	bool handleWrite();
	bool shouldClose() const;
	void prepareResponse();
	// Serialize the current response into _rawResponse without
	// re-running the response builder. Caller must have set status
	// (and optionally body) on the response. Used by the CGI error
	// path so a failed CGI doesn't re-trigger routing.
	void prepareResponseDirect();
	void forceTimeoutError();

	// --- CGI streaming hooks ---
	// Append a pre-framed chunk of wire bytes to the outgoing stream
	// buffer (used while a CGI is producing output). For STREAMING_CGI
	// connections this is the queue that handleWrite() drains into the
	// socket.
	void appendToStreamBuffer(const char *data, size_t n);
	void appendToStreamBuffer(const std::string &s);
	// Current size of the stream buffer (for back-pressure decisions).
	size_t streamBufferSize() const;
	// Mark the CGI side as fully drained (terminator already queued).
	// handleWrite() returns "finished" once the buffer empties and
	// this flag is set.
	void markStreamFinished();
	bool isStreamFinished() const;
	// True once handleWrite() has flushed the terminator and the
	// stream is fully done. After this the surrounding loop runs the
	// same keep-alive / close logic as a static response.
	bool isStreamDrained() const;
	// True if the stream had to be aborted post-headers (e.g. CGI
	// crashed mid-response). The connection MUST close — we can't
	// send a 500 because headers are already on the wire.
	void markStreamAborted();
	bool isStreamAborted() const;

	void resetForNextRequest();
	void setCgiPid(pid_t pid);
	static const int MAX_HEADER_SIZE = 16384; // 16KB

	bool isChunked() const;

private:
	Connection(const Connection &other);
	Connection &operator=(const Connection &other);

	ConnectionState _state;
	int _connectFd;
	Listener *_listener; // for getting  client_max_body_size from the server config

	std::string _rawRequest;
	std::string _chunkedAccumulator;
	std::string _rawResponse;

	// Streaming queue used only while _state == STREAMING_CGI. Bounded
	// in size by the WebServ::CGI_FORWARD_HIGH_WATER back-pressure
	// gate. Erase-from-front semantics, so we never carry more than a
	// few tens of KB per in-flight CGI response.
	std::string _streamBuf;
	bool _streamFinished; // terminator queued
	bool _streamDrained;  // buffer flushed AND _streamFinished
	bool _streamAborted;  // post-header crash → close after drain

	size_t _expectedBodySize;
	bool _isChunked;
	size_t _bytesSent;
	time_t _lastActive;

	HTTP_ResponseBuilder _responseBuilder;
	HTTP_Request _request;
	HTTP_Response _response;

	pid_t _cgi_pid;

	void _handleHeaders();
	void _setupBodyReading();
	void _handleStandardBody();
	void _handleChunkedBody();
	bool _isValidHex(const std::string &s) const;
};

#endif
