/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:02:21 by aistok            #+#    #+#             */
/*   Updated: 2026/05/29 14:31:01 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <csignal>
#include <clocale>

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

bool guarantee_english_locale_time(bool setup)
{
	static std::string saved_locale;
	static bool initialized = false;

	if (setup)
	{
		char* old_locale = std::setlocale(LC_TIME, NULL);
		saved_locale = old_locale ? old_locale : "C";

		// Enforce standard formatting rules
		std::setlocale(LC_TIME, "C");
		initialized = true;
		return (true);
	}

	// !setup = restore/revert
	// only if this function saved previous state already
	if (!initialized)
		return (false);
	
	std::setlocale(LC_TIME, saved_locale.c_str());
	initialized = false;
	return (true);
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

	bool test_mode = false;

	signal(SIGPIPE, SIG_IGN);	  // Prevents crash on broken pipe
	signal(SIGINT, handleSigint); // Handles Ctrl+C

	// if (argc > 2)
	// {
	// 	std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
	// 	return 1;
	// }
	// std::string configPath = (argc == 2) ? argv[1] : "../default.conf";
	if (argc == 3 && std::string(argv[1]) == "--check") 
	{
		test_mode = true;
		config_file = argv[2];
	}
	else if (argc == 2)
		config_file = argv[1];

	guarantee_english_locale_time(true); // AI: added for later calls to Utils::getHttpDate()

	try
	{
		Config config;
		config.load(config_file);
		// FOR TESTING (getMockConfig() should be replaced by ConfigParser)
		// std::vector<ServerConfig> configs = getMockConfig();

		if (test_mode) {
			std::cout << "Config OK\n";
			return (0);
		}
		
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
		guarantee_english_locale_time(false);
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}
	
	guarantee_english_locale_time(false);
	return (0);
}
