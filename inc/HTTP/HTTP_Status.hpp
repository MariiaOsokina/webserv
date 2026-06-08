/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Status.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/12 14:37:18 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:22:36 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_STATUS_HPP
#define HTTP_STATUS_HPP

typedef struct HTTP_StatusPair
{
	int code;
	const char *text;

	bool operator==(const HTTP_StatusPair &other) const;
} HTTP_StatusPair;

class HTTP_Status
{
public:
	static const HTTP_StatusPair UNSET;
	static const HTTP_StatusPair s0;

	/* 1xx Informational */
	static const HTTP_StatusPair CONTINUE;
	static const HTTP_StatusPair s100;

	/* 2xx Success */
	static const HTTP_StatusPair OK;
	static const HTTP_StatusPair s200;

	static const HTTP_StatusPair CREATED;
	static const HTTP_StatusPair s201;

	static const HTTP_StatusPair ACCEPTED;
	static const HTTP_StatusPair s202;

	static const HTTP_StatusPair NO_CONTENT;
	static const HTTP_StatusPair s204;

	/* 3xx Redirections */
	static const HTTP_StatusPair MOVED_PERMANENTLY;
	static const HTTP_StatusPair s301;

	static const HTTP_StatusPair FOUND;
	static const HTTP_StatusPair s302;

	static const HTTP_StatusPair SEE_OTHER;
	static const HTTP_StatusPair s303;

	static const HTTP_StatusPair NOT_MODIFIED;
	static const HTTP_StatusPair s304;

	static const HTTP_StatusPair INTERNAL_REDIRECT;
	static const HTTP_StatusPair s307; // RFC 9110 (RFC 2616)

	static const HTTP_StatusPair PERMANENT_REDIRECT;
	static const HTTP_StatusPair s308; // RFC 9110 (RFC 7538)

	/* 4xx Client Errors */
	static const HTTP_StatusPair BAD_REQUEST;
	static const HTTP_StatusPair s400;

	static const HTTP_StatusPair UNAUTHORIZED;
	static const HTTP_StatusPair s401;

	static const HTTP_StatusPair FORBIDDEN;
	static const HTTP_StatusPair s403;

	static const HTTP_StatusPair NOT_FOUND;
	static const HTTP_StatusPair s404;

	static const HTTP_StatusPair METHOD_NOT_ALLOWED;
	static const HTTP_StatusPair s405;

	static const HTTP_StatusPair REQUEST_TIMEOUT;
	static const HTTP_StatusPair s408;

	static const HTTP_StatusPair CONFLICT;
	static const HTTP_StatusPair s409;

	static const HTTP_StatusPair LENGTH_REQUIRED;
	static const HTTP_StatusPair s411;

	static const HTTP_StatusPair CONTENT_TOO_LARGE;
	static const HTTP_StatusPair s413;

	static const HTTP_StatusPair IM_A_TEAPOT;
	static const HTTP_StatusPair s418;

	static const HTTP_StatusPair UNPROCESSABLE_CONTENT;
	static const HTTP_StatusPair s422;

	static const HTTP_StatusPair REQUEST_HEADER_FIELDS_TOO_LARGE;
	static const HTTP_StatusPair s431;

	/* 5xx Server Errors */
	static const HTTP_StatusPair INTERNAL_SERVER_ERROR;
	static const HTTP_StatusPair s500;

	static const HTTP_StatusPair NOT_IMPLEMENTED;
	static const HTTP_StatusPair s501;

	static const HTTP_StatusPair BAD_GATEWAY;
	static const HTTP_StatusPair s502;

	static const HTTP_StatusPair SERVICE_UNAVAILABLE;
	static const HTTP_StatusPair s503;

	static const HTTP_StatusPair GATEWAY_TIMEOUT;
	static const HTTP_StatusPair s504;

	static const HTTP_StatusPair HTTP_VERSION_NOT_SUPPORTED;
	static const HTTP_StatusPair s505;

	static const HTTP_StatusPair *all();
	static int count();

	static const HTTP_StatusPair &fromCode(int code);

private:
	// Private and Unimplemented to prevent copying and class instantiation
	HTTP_Status();
	~HTTP_Status();
	HTTP_Status(const HTTP_Status &other);
	HTTP_Status &operator=(const HTTP_Status &other);

	static const HTTP_StatusPair _table[];
	static const int _count;

	friend class HTTP;
	friend class HTTP_Request;
	friend class HTTP_Response;
	friend class HTTP_ResponseBuilder;
};

#endif // HTTP_STATUS_HPP
