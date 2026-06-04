/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MockTestFunctions.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/11 12:49:30 by mosokina          #+#    #+#             */
/*   Updated: 2026/06/04 16:47:44 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "MockTestFnctions.hpp"

// FOR TESTING (runTemporaryTest() should be replaced by run() with poll() approach):
void runTemporaryTest(WebServ &ws)
{
	if (ws.getListeners().empty())
	{
		std::cerr << "Error: No servers initialized to test." << std::endl;
		return;
	}

	Listener *firstListener = ws.getListeners()[0];
	int listenFd = firstListener->getListenFd();

	// Safety check: Did the socket actually open?
	if (listenFd == -1)
	{
		std::cerr << "Error: Listen FD is invalid (-1). Check bind errors." << std::endl;
		return;
	}

	std::cout << "--- [STARTING TEMPORARY TEST] ---" << std::endl;
	std::cout << "Waiting for telnet on the first configured port..." << std::endl;

	sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	while (true)
	{
		int clientFd = accept(listenFd, (sockaddr *)&clientAddr, &clientLen);

		if (clientFd == -1)
		{
			// Since it's O_NONBLOCK, we sleep to prevent 100% CPU usage
			usleep(10000);
			continue;
		}

		std::cout << "Connection accepted! FD: " << clientFd << std::endl;

		// 1. Send the greeting
		std::string msg = "Hello World! Type something and hit Enter:\n";
		send(clientFd, msg.c_str(), msg.length(), 0);

		// 2. Read user input
		char buffer[1024] = {0};
		int bytesRead = recv(clientFd, buffer, 1023, 0); // 1023 to save space for null

		if (bytesRead > 0)
		{
			buffer[bytesRead] = '\0'; // Null-terminate for safe printing
			std::cout << "Received from client: " << buffer << std::endl;
		}

		// 3. Close the connection
		close(clientFd);
		std::cout << "Connection closed. Ready for next test..." << std::endl;
	}
}

/*JUST FOR TESTING (getMockConfig() should be replaced by ConfigParser)*/
std::vector<ServerConfig> getMockConfig()
{
	std::vector<ServerConfig> configs;

	// --- SERVER 1 (Port 8080) ---
	ServerConfig s1;
	s1.ports.push_back(8080);
	s1.host = "127.0.0.1";
	s1.client_max_body_size = 1048576; // 1MB

	// Location / (Root)
	LocationConfig loc1;
	loc1.path = "/";
	loc1.root = "./www/html";
	loc1.index = "index.html";
	loc1.allowed_methods.push_back("GET");
	loc1.autoindex = true;
	s1.locations.push_back(loc1);

	// Location / (Root)
	LocationConfig loc2;
	loc2.path = "/YoupiBanane";
	loc2.root = "./www/html";
	loc2.index = "banane.html";
	loc2.allowed_methods.push_back("GET");
	loc2.autoindex = true;
	s1.locations.push_back(loc2);

	configs.push_back(s1);

	// --- SERVER 2 (Port 9090) ---
	ServerConfig s2;
	s2.ports.push_back(9090);
	s2.host = "127.0.0.1";
	s2.client_max_body_size = 5000; // Small limit for testing 413 errors

	// Location /uploads
	LocationConfig loc10;
	loc10.path = "/uploads";
	loc10.root = "./www/uploads";
	loc10.allowed_methods.push_back("POST");
	loc10.allowed_methods.push_back("DELETE");
	s2.locations.push_back(loc10);

	configs.push_back(s2);

	return configs;
}
