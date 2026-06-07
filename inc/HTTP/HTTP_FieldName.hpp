/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_FieldName.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/12 14:37:18 by aistok            #+#    #+#             */
/*   Updated: 2026/06/06 20:19:37 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_FIELDNAME_HPP
#define HTTP_FIELDNAME_HPP

#include <string>

/*
 * Minimal set of headers in a request
 * (sent by the client to the server):
 *
 * 	GET / HTTP/1.1		request line !
 *	Host				required in HTTP/1.1
 *	Content-Length		only if the request has a body
 *	Transfer-Encoding	chunked
 *						NOTES:
 * 						appears, when the client is sending data to the
 *						server, and it does not know the length of the
 *						content up front (ex: streaming video or audio);
 *						IMPORTANT: this may be a security vulnerability
 *						if it appears beside Content-Length! In this case,
 *						the request is rejected!
 *						https://portswigger.net/web-security/request-smuggling
 *
 *	Content-Type		only if the request has a body (e.g. POST/PUT)
 *
 *	User-Agent			optional (very common, not required)
 *	Accept				optional
 *	Range				optional (only when requesting partial content)
 *	Connection			optional (e.g. keep-alive, close)
 */

/*
 * Minimal set of headers in a response
 * (sent by the server to the client):
 *
 * 	HTTP/1.1 200 OK		status line (or response line)
 *	Date				yes, response header (SHOULD be sent by origin servers)
 *
 *	Content-Type		if the response has a body
 *	Content-Length		if the response has a body AND IS NOT CHUNKED !
 *	Transfer-Encoding	appears, when the server is sending data to the
 *						client, and it does not know the length upfront;
 *						IMPORTANT SECURITY RISK, see above!
 *
 *	Content-Range		when returning partial content, sample value:
 *						bytes 0-99/1234
 *	Connection			optional (often mirrors request intent)
 */

class HTTP_FieldName
{
public:
	/* Used in Request & Response headers */
	static const std::string CONTENT_LENGTH;
	static const std::string TRANSFER_ENCODING;
	static const std::string CONTENT_TYPE;

	/* Used only in request headers */
	static const std::string HOST;
	static const std::string RANGE;

	static const std::string USER_AGENT;
	static const std::string CONNECTION;

	/* Used only in response headers */
	static const std::string SERVER_NAME;
	static const std::string DATE;
	static const std::string CONTENT_RANGE;
	static const std::string LOCATION;

	/* the below is not needed at the moment */
	// friend class HTTP;
	// friend class HTTP_Request;
};

#endif // HTTP_FIELDNAME_HPP
