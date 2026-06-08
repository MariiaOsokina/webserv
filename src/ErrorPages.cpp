/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorPages.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/13 03:08:23 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 18:44:40 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Utils.hpp"
#include "Config.hpp"
#include "ErrorPages.hpp"

#include <map>
#include <sstream>
#include <exception>

std::string ErrorPages::_template =
	"<!DOCTYPE html>\n"
	"<html lang=\"en\">\n"
	"<head>\n"
	"  <meta charset=\"UTF-8\">\n"
	"  <title>{{STATUS_CODE}} {{STATUS_MESSAGE}}</title>\n"
	"  <style>\n"
	"    body { font-family: Arial, sans-serif; background: #f4f4f4; }\n"
	"    .container { margin: 100px auto; width: 600px; text-align: center; }\n"
	"    h1 { font-size: 72px; margin-bottom: 10px; }\n"
	"    p { font-size: 24px; color: #555; }\n"
	"  </style>\n"
	"</head>\n"
	"<body>\n"
	"  <div class=\"container\">\n"
	"    <h1>{{STATUS_CODE}}</h1>\n"
	"    <p>{{STATUS_MESSAGE}}</p>\n"
	"  </div>\n"
	"</body>\n"
	"</html>\n";

ErrorPages::ErrorPages() {}

ErrorPages::~ErrorPages() {}

std::string ErrorPages::generate(const HTTP_StatusPair &status)
{
	std::stringstream ss;
	ss << status.code;

	std::string result = _template;
	result = Utils::replaceAll(result, "{{STATUS_CODE}}", ss.str());
	result = Utils::replaceAll(result, "{{STATUS_MESSAGE}}", status.text);

	return result;
}

std::string ErrorPages::getContent(const ServerConfig &sc, const HTTP_StatusPair &status)
{
	if (sc.error_pages.find(status.code) != sc.error_pages.end())
	{
		// we should have a custom error page in the ServerConfig
		// try and use that one
		std::string customErrorPagePath;
		std::map<int, std::string>::const_iterator it = sc.error_pages.begin();
		for (; it != sc.error_pages.end(); ++it)
		{
			if (it->first == status.code)
			{
				customErrorPagePath = it->second;
				break;
			}
		}

		std::string pathOnServer = sc.root + customErrorPagePath;
		if (getPathType(pathOnServer) == PATH_FILE)
		{
			try
			{
				std::string content = Utils::getFileContent(pathOnServer);
				return (content);
			}
			catch (std::exception &e)
			{
				// Fall back to built-in error page below
			}
		}
	}

	// on failure to access the custom error page
	// or if there is no custom error page in the ServerConfig
	// just generate a default one
	return (generate(status));
}
