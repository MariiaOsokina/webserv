/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/11 12:49:22 by mosokina          #+#    #+#             */
/*   Updated: 2026/05/22 12:30:12 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <iostream>
#include <unistd.h> // close
#include <ctime>
#include <cstdlib>
#include <signal.h>   // For SIGKILL
#include <sys/wait.h> // For waitpid

#include "Listener.hpp"
#include "HTTP/HTTP.hpp"

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
		ERROR
	};
	
	void resetTimeout();
	bool isTimedOut(time_t now, int limit) const;
	
	HTTP::Request &getRequest();
	HTTP::Response &getResponse();
	Listener *getServer();
	int getState() const;
	void setState(ConnectionState state); // <MO: new for CGI
    std::string getRawRequest() const;
	std::string getRawResponse() const;
	bool hasBufferedData() const ;

	void handleRead(const char *buffer, ssize_t bytesRead);
	bool handleWrite();
	bool shouldClose() const;
	void prepareResponse();
	void forceTimeoutError();


	void resetForNextRequest();
	void setCgiPid(pid_t pid);
	static const int MAX_HEADER_SIZE = 16384; // 16KB

	bool isChunked() const; // AI: added


private:
	Connection(const Connection &other);
	Connection &operator=(const Connection &other);

	ConnectionState _state;
	int _connectFd;
	Listener *_listener; // for getting  client_max_body_size from the server config
	
	std::string _rawRequest;
	std::string	_chunkedAccumulator;
	std::string _rawResponse;


	size_t	_expectedBodySize;
    bool _isChunked;
	size_t _bytesSent;
	time_t _lastActive;

	HTTP::ResponseBuilder _responseBuilder;
	HTTP::Request _request;
	HTTP::Response _response;
	
    pid_t _cgi_pid; // Add this!
	// sockaddr_in _clientAddr;
	// std::string _clientIP;

	void _handleHeaders();
	void _setupBodyReading();
    void _handleStandardBody();
    void _handleChunkedBody();
	bool _isValidHex(const std::string& s) const;


};

#endif