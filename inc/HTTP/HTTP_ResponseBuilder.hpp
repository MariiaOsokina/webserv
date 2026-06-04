/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_ResponseBuilder.hpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/20 10:48:39 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 14:35:15 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSEBUILDER_HPP
#define HTTP_RESPONSEBUILDER_HPP

#include "Utils.hpp"
#include "Config.hpp"
#include "HTTP/HTTP_Request.hpp"
#include "HTTP/HTTP_Response.hpp"
#include "ErrorPages.hpp"
#include "DirectoriesToHTML.hpp"
#include "CGI.hpp"

#include <exception>
#include <string>

class HTTP_ResponseBuilder
{
public:
	class Exception : public std::exception
	{
	public:
		Exception(const HTTP_StatusPair &status, const std::string &msg);

		virtual ~Exception() throw();

		virtual const char *what() const throw();

		HTTP_StatusPair getStatus() const;

	private:
		HTTP_StatusPair _status;
		std::string _message;
	};

	HTTP_ResponseBuilder();
	HTTP_ResponseBuilder(const ServerConfig &sc);
	~HTTP_ResponseBuilder();

	void build(HTTP_Response &response, HTTP_Request &request);
	void reset();

	static ssize_t getClientMaxBodySize(const ServerConfig &sc, const HTTP_Request &req, HTTP_Response &res);

	// Returns the effective client_max_body_size for the request:
	// the matching location's value if set, otherwise the server-level
	// value. Returns 0 if neither is set, or if no location matches
	// (caller should fall back to server-level semantics; the 404 is
	// produced later by build()). No side effects on req/sc.
	static size_t resolveBodyLimit(const ServerConfig &sc, const HTTP_Request &req);

private:
	ServerConfig _serverConfig;
	LocationConfig _location;
	std::string _pathOnServer;
	PathType _pathType;

	HTTP_ResponseBuilder(const HTTP_ResponseBuilder &other);
	HTTP_ResponseBuilder &operator=(const HTTP_ResponseBuilder &other);

	void build_response_for_GET_or_HEAD(HTTP_Response &response, const HTTP_Request &request);
	void build_response_for_POST(HTTP_Response &response, HTTP_Request &request);
	void build_response_for_DELETE(HTTP_Response &response, const HTTP_Request &request);

	bool locationHasMethod(const LocationConfig &location, const std::string &method);
	static const LocationConfig &locationGetBestMatch(const ServerConfig &sc, const HTTP_Request &req);
	const LocationConfig &locationGetBestMatch(const HTTP_Request &request);
	std::string translateUriToPath(const HTTP_Request &request);

	void setResponse(HTTP_Response &response, const HTTP_StatusPair &status);
	static void setResponse(HTTP_Response &response, const HTTP_StatusPair &status, const ServerConfig &sc);

	static void setResponseRedirect(HTTP_Response &response, const int statusCode, const std::string &url);
};

#endif // HTTP_RESPONSEBUILDER_HPP
