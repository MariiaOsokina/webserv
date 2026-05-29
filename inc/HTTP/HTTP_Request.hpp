/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Request.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/16 16:34:38 by aistok            #+#    #+#             */
/*   Updated: 2026/05/29 18:02:16 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <iostream>
#include <map>
#include <strings.h> // For strcasecmp
#include <sstream> // stringbuf
#include <istream> // istream

#include "HTTP_Version.hpp"
#include "HTTP_Status.hpp"
#include "HTTP_Method.hpp"
#include "HTTP_FieldName.hpp"
#include "Utils.hpp"

struct CaseInsensitiveCompare {
	bool operator()(const std::string& a, const std::string& b) const {
		// strcasecmp returns 0 if strings are equal (ignoring case)
		// std::map needs a "less than" comparison, so we return true if a < b
		return strcasecmp(a.c_str(), b.c_str()) < 0;
	}
};

typedef std::map<std::string, std::string, CaseInsensitiveCompare> HTTP_Headers;

class HTTP_Request
{
public:
	HTTP_Request();
	HTTP_Request(const char *raw, size_t len);
	HTTP_Request(const HTTP_Request &other);
	HTTP_Request &operator=(const HTTP_Request &other);
	~HTTP_Request();

	enum ParseStatus
	{
		INCOMPLETE = 0,
		COMPLETE = 1,
		BAD_REQUEST = 400,
		REQUEST_TIMEOUT = 408,
		CONTENT_TOO_LARGE = 413,
		REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
		NOT_IMPLEMENTED = 501,
		HTTP_VERSION_NOT_SUPPORTED = 505,
	};

	enum ParsingState
	{
		STATE_REQUEST_LINE, // Parsing: GET /index.html HTTP/1.1
		STATE_HEADERS,      // Parsing: Host: localhost...
		STATE_BODY,         // Parsing: Binary data or form data
		STATE_COMPLETE,     // Entire request is ready
		STATE_ERROR         // Something went wrong (e.g., 400 Bad Request)
	};


	int parseHeaders(const char *raw, size_t len);
	void setBody(const std::string &data, size_t len);
	void appendToBody(const std::string &data, size_t len, bool isFinalAppend);

	int getParseStatus() const;
	const std::string &getMethod() const;
	const std::string &getURL() const;
	std::string getURLWithoutParams() const;
	std::string getQueryString() const;
	const std::string &getVersion() const;
	const std::map<std::string, std::string, CaseInsensitiveCompare> getHeaders() const;
	const std::string &getBody() const;
	// Swap the request body with `dst`, transferring ownership in O(1)
	// (no copy). Used to hand a large request body to the CGI worker
	// without doubling memory — once CGI is started the Connection does
	// not need the body anymore.
	void swapBody(std::string &dst);
	std::string serialize() const;
	
	bool isMultipartRequest() const;
	const std::string getMultipartBoundary() const;
	int populateMultipartVars();

	bool ready();
	void reset();
	void setParseStatus(const ParseStatus status);
	HTTP_Request getDisplayFriendlyRequest() const;
	bool hasHeader(const std::string &fieldName) const;
	void dumpToFile(const std::string &filename) const;

protected:
	// ...

private:
	bool _isDisplayFriendlyRequest;
	std::string _method;
	std::string _url;
	std::string _version;
	bool _requestLine_completed;

	/* * RFC 9110 Compliance: HTTP header names are case-insensitive. 
	* Using CaseInsensitiveCompare ensures that "Content-Type" and "content-type" 
	* are treated as the same key, preventing duplicate entries and lookup failures.
	*/
	HTTP_Headers _headers;
	bool _headers_completed;
	int _headersRequiredCount;
	
	bool _isMultipartRequest;
	std::string _multipartBoundary;
	std::string _multipartFilename;
	std::string _multipartContentType;
	std::string _multipartData;

	std::string _body;
	bool _body_completed;

	ParseStatus _parseStatus;

	void _init_class_vars();
	void _set_class_vars(const HTTP_Request &other);

	int _parseRequestLine(std::string &line);
	int _parseMethod(const std::string &method);
	int _parseURL(const std::string &url);
	int _parseVersion(const std::string &version);
	int _URLIsValid(const std::string &url);

	int _parseHeaderLine(std::string &line);
	int _countHeaderIfRequired(const std::string &fieldName);
	int _fieldNameIsValid(const std::string &fieldName);
	int _headerValueIsValid(const std::string &value);
	int _fieldNameAlreadyProcessed(const std::string &eKey);
	int _fieldNameIsSecurityRisk(const std::string &eKey);
	HTTP_Headers _parseMultipartHeaders(const std::string &multipartHeadersStr);
	std::string _extractFromValue(const std::string &prefix, const std::string &dataString);

	// only if need access to private or protected elements
	friend class HTTP;
	friend class HTTP_ResponseBuilder; // FOR DEBUG ONLY!!! TO-DO: REMOVE!
	friend std::ostream &operator<<(std::ostream &os, const HTTP_Request &hr);
};

//std::ostream &operator<<(std::ostream &os, const HTTP_Request &hr);

#endif // HTTP_REQUEST_HPP
