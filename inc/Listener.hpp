/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/23 14:46:16 by mosokina          #+#    #+#             */
/*   Updated: 2026/04/01 20:04:16 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
#define LISTENER_HPP

// TO-DO update based on func used in the code!!

#include <arpa/inet.h>	// htons
#include <fcntl.h>		// fcntl, O_NONBLOCK
#include <netdb.h>		// getaddrinfo, freeaddrinfo
#include <sys/socket.h> // socket, bind, listen
#include <sys/types.h>	// types for sockets
#include <unistd.h>		// close
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <cstring>

#include "Config.hpp"
#include "Utils.hpp"

class Listener
{
public:
	Listener(const ServerConfig &config, int port);
	~Listener();

	void initSocket(); // bind(), socket setup
	int getListenFd() const;
	const ServerConfig &getConfig() const;

private:
	// Rule of Three: Private and Unimplemented to prevent copying
	Listener(const Listener &other);
	Listener &operator=(const Listener &other);

	sockaddr_in _getSocketAddress(const std::string &host, int port) const;

	ServerConfig _config;
	int _port;
	int _listenFd;
	sockaddr_in _address;

	static const int BACKLOG = 128; // limit of pending connections in the socket's listen queue
};

#endif
