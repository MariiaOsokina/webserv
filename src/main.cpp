/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:02:21 by aistok            #+#    #+#             */
/*   Updated: 2026/05/26 12:54:00 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <csignal>

#include "WebServ.hpp"
#include "Listener.hpp"
#include "Config.hpp"

// #include "MockTestFnctions.hpp"

/*How to Test:
1 - Run it: ./webserv
2 - Open a new terminal and type: telnet localhost 8080 (or whatever port you used).
3 - TO BE ADDED.*/

volatile sig_atomic_t g_server_running = 1;
void handleSigint(int sig)
{
	(void)sig;
	g_server_running = 0;
}

int main(int argc, char **argv)
{
	// TEMPORARILY, FOR CONFIG TESTING ONLY
	std::string config_file = "config/advanced.conf";
	// std::string config_file = "config/advanced.conf";
	// std::string config_file = "config/test_duplicate_directive.conf";
	// std::string config_file = "config/test_invalid_method.conf";
	// std::string config_file = "config/test_missing_brace.conf";
	// std::string config_file = "config/test_missing_semicolon.conf";

	signal(SIGPIPE, SIG_IGN);	  // Prevents crash on broken pipe
	signal(SIGINT, handleSigint); // Handles Ctrl+C

	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
		return 1;
	}
	// std::string configPath = (argc == 2) ? argv[1] : "../default.conf";
	if (argc == 2)
		config_file = argv[1];
	try
	{
		Config config;
		config.load(config_file);
		// FOR TESTING (getMockConfig() should be replaced by ConfigParser)
		// std::vector<ServerConfig> configs = getMockConfig();

		WebServ ws;
		// ws.setup(configs);
		ws.setup(config.getServers());

		ws.run();
		// FOR TESTING (runTemporaryTest() should be replaced by run() with poll() approach):
		// runTemporaryTest(ws);
		std::cout << "\nServer shutting down cleanly..." << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}
	return (0);
}
