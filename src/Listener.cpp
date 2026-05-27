/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/23 14:48:56 by mosokina          #+#    #+#             */
/*   Updated: 2026/04/01 20:04:27 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Listener.hpp"

Listener::Listener(const ServerConfig &config, int port) : _config(config), _port(port), _listenFd(-1)
{
	_address = sockaddr_in();
}

Listener::~Listener()
{
	if (_listenFd != -1)
	{
		std::cout << "Closing fd " << _listenFd << std::endl;
		close(_listenFd);
		_listenFd = -1;
	}
}

int Listener::getListenFd() const
{
	return _listenFd;
}

const ServerConfig &Listener::getConfig() const
{
	return _config;
}

void Listener::initSocket()
{
	// 1. Create the socket (IPv4, TCP)
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd == -1)
	{
		throw std::runtime_error("Failed to open listen fd");
	}
	// 2. Set SO_REUSEADDR (Allows immediate restart of the listener)
	int opt = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		close(_listenFd);
		_listenFd = -1;
		throw std::runtime_error("Failed to set setsockopt");
	}
	// 3. Set to Non-Blocking (Crucial for WebServ requirements)
	if (setNonBlocking(_listenFd) == false)
	{
		close(_listenFd);
		_listenFd = -1;
		throw std::runtime_error("Failed to set non-blocking mode");
	}
	// 4. Prepare address structure
	_address = _getSocketAddress(_config.host, _port);
	// 5. Bind socket to the address/port
	if (bind(_listenFd, (sockaddr *)&_address, sizeof(_address)) == -1)
	{
		close(_listenFd);
		_listenFd = -1;
		throw std::runtime_error("Failed to bind to port " + toString(_port) + ": " + std::strerror(errno));
	}
	// 6. Start listening for incoming connections
	if (listen(_listenFd, BACKLOG) == -1)
	{
		close(_listenFd);
		_listenFd = -1;
		throw std::runtime_error("Failed to listen on socket");
	}
	std::cout << "[Listener] Listening on " << _config.host << ":" << _port << std::endl;
}

// Helper function to resolve host/port to sockaddr_in
sockaddr_in Listener::_getSocketAddress(const std::string &host, int port) const
{
	addrinfo hints;
	addrinfo *result = NULL;
	sockaddr_in addr;

	std::memset(&hints, 0, sizeof(hints));
	std::memset(&addr, 0, sizeof(addr));
	hints.ai_family = AF_INET;		 // Force IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP

	std::string portStr = toString(port);

	const char *hostPtr = (host.empty() || host == "0.0.0.0") ? NULL : host.c_str();
	if (hostPtr == NULL)
	{
		hints.ai_flags = AI_PASSIVE; // for binding to all interfaces
	}
	int status = getaddrinfo(hostPtr, portStr.c_str(), &hints, &result);
	if (status != 0)
	{
		throw std::runtime_error("DNS Error: " + std::string(gai_strerror(status)));
	}
	if (result && result->ai_addr)
	{
		addr = *(sockaddr_in *)result->ai_addr;
	}
	else
	{
		if (result)
			freeaddrinfo(result);
		throw std::runtime_error("DNS Error: No address found for " + host);
	}
	freeaddrinfo(result);
	return addr;
}
