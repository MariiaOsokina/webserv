/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:02:21 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 14:28:43 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServ.hpp"
#include "Listener.hpp"
#include "Config.hpp"

#include <iostream>
#include <csignal>
#include <clocale>

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
		char *old_locale = std::setlocale(LC_TIME, NULL);
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
	std::string config_file = "config/advanced.conf";
	bool test_mode = false;

	signal(SIGPIPE, SIG_IGN);	  // Prevents crash on broken pipe
	signal(SIGINT, handleSigint); // Handles Ctrl+C

	if (argc == 3 && std::string(argv[1]) == "--check")
	{
		test_mode = true;
		config_file = argv[2];
	}
	else if (argc == 2)
	{
		config_file = argv[1];
	}

	guarantee_english_locale_time(true);

	try
	{
		Config config;
		config.load(config_file);

		if (test_mode)
		{
			std::cout << "Config OK\n";
			return (0);
		}

		WebServ ws;
		ws.setup(config.getServers());

		ws.run();
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
