/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_FieldName.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/12 14:37:18 by aistok            #+#    #+#             */
/*   Updated: 2026/06/06 20:49:31 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP_FieldName.hpp"

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

/* Used in Request & Response headers */
const std::string HTTP_FieldName::CONTENT_LENGTH = "Content-Length";
const std::string HTTP_FieldName::TRANSFER_ENCODING = "Transfer-Encoding";
const std::string HTTP_FieldName::CONTENT_TYPE = "Content-Type";

/* Used only in request headers */
const std::string HTTP_FieldName::HOST = "Host";
const std::string HTTP_FieldName::RANGE = "Range";

const std::string HTTP_FieldName::USER_AGENT = "User-Agent";
const std::string HTTP_FieldName::CONNECTION = "Connection";

/* Used only in response headers */
const std::string HTTP_FieldName::SERVER_NAME = "Server";
const std::string HTTP_FieldName::DATE = "Date";
const std::string HTTP_FieldName::CONTENT_RANGE = "Content-Range";
const std::string HTTP_FieldName::LOCATION = "Location";
