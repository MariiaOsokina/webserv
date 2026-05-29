/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Request.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/16 16:46:32 by aistok            #+#    #+#             */
/*   Updated: 2026/05/29 17:02:22 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP/HTTP_Request.hpp"

void HTTP_Request::_init_class_vars()
{
	_isDisplayFriendlyRequest = false;

	_method = "";
	_url = "";
	_version = "";
	_requestLine_completed = false;

	_headers.clear();
	_headers_completed = false;
	_headersRequiredCount = 0;

	_isMultipartRequest = false;
	_multipartBoundary = "";
	_multipartFilename = "";
	_multipartContentType = "";
	_multipartData = "";

	_body = "";
	_body_completed = false;

	_parseStatus = INCOMPLETE;
}

void HTTP_Request::_set_class_vars(const HTTP_Request &other)
{
	_isDisplayFriendlyRequest = other._isDisplayFriendlyRequest;

	_method = other._method;
	_url = other._url;
	_version = other._version;
	_requestLine_completed = other._requestLine_completed;

	_headers = other._headers;
	_headers_completed = other._headers_completed;
	_headersRequiredCount = other._headersRequiredCount;

	_isMultipartRequest = other._isMultipartRequest;
	_multipartBoundary = other._multipartBoundary;
	_multipartFilename = other._multipartFilename;
	_multipartContentType = other._multipartContentType;
	_multipartData = other._multipartData;

	_body = other._body;
	_body_completed = other._body_completed;

	_parseStatus = other._parseStatus;
}

HTTP_Request::HTTP_Request()
{
	_init_class_vars();
}

HTTP_Request::HTTP_Request(const char *raw, size_t len)
{
	_init_class_vars();
	parseHeaders(raw, len);
}

HTTP_Request::~HTTP_Request() {}

HTTP_Request::HTTP_Request(const HTTP_Request &other)
{
	if (this != &other)
		_set_class_vars(other);
}

HTTP_Request &HTTP_Request::operator=(const HTTP_Request &other)
{
	if (this != &other)
		_set_class_vars(other);

	return (*this);
}

/* getline removes the '\n' from each line it reads! */
int HTTP_Request::parseHeaders(const char *raw, size_t len) // MO:SPLIT TO parseHeaders and setBody() -> AI: appendToBody() instead of setBody() ?
{
	if (!len)
	{
		_parseStatus = HTTP_Request::INCOMPLETE;
		return (FAILURE);
	}

	std::string rawStr(raw, len);
	std::stringbuf sb(rawStr);
	std::istream is(&sb);
	std::string line;

	if (!_requestLine_completed)
	{
		if (!std::getline(is, line))
		{
			/* no data available
			 * QUESTION: is this even possible? for '\n' to be missing?
			 */
			this->_parseStatus = HTTP_Request::INCOMPLETE;
			return (FAILURE);
		}
		if (!_parseRequestLine(line))
			/* request line was malformed?
			 * parseStatus will be set!
			 */
			return (FAILURE);

		_requestLine_completed = true;
	}

	while (!_headers_completed && std::getline(is, line))
	{
		if (line == CR) /* LF was removed by getline! */
		{
			/* reached the end of headers;
			 * body may be present after this
			 */
			if (_headersRequiredCount < 2)
			{
				_parseStatus = HTTP_Request::BAD_REQUEST;
				return (FAILURE);
			}
			_headers_completed = true;
			_parseStatus = HTTP_Request::COMPLETE; // Headers are done!
			return (SUCCESS);
		}
		else
		{
			if (!HTTP_Request::_parseHeaderLine(line))
				/* malformed header line
				 * parseStatus is now set accordingly!
				 */
				return (FAILURE);
		}
	}

	// 3. Fallback: If we ran out of string before finding the empty line
	_parseStatus = HTTP_Request::INCOMPLETE;
	return (FAILURE);
}

int HTTP_Request::getParseStatus() const
{
	return (_parseStatus);
}

const std::string &HTTP_Request::getMethod() const
{
	return (_method);
}

const std::string &HTTP_Request::getURL() const
{
	return (_url);
}
std::string HTTP_Request::getURLWithoutParams() const
{
	size_t query_pos = _url.find('?');
	if (query_pos != std::string::npos)
		return (_url.substr(0, query_pos));

	return (_url);
}
std::string HTTP_Request::getQueryString() const
{
	size_t queryStartPos = _url.find('?');

	if (queryStartPos != std::string::npos)
		return (_url.substr(queryStartPos + 1));

	return ("");
}
const std::string &HTTP_Request::getVersion() const
{
	return (_version);
}

const std::map<std::string, std::string, CaseInsensitiveCompare> HTTP_Request::getHeaders() const
{
	return (_headers);
}

const std::string &HTTP_Request::getBody() const
{
	return (_body);
}

void HTTP_Request::swapBody(std::string &dst)
{
	_body.swap(dst);
}

std::string HTTP_Request::serialize() const
{
	std::ostringstream oss(std::ios::binary);
	oss << *this;
	return (oss.str());
}

bool HTTP_Request::isMultipartRequest() const
{
	return (_isMultipartRequest);
}

const std::string HTTP_Request::getMultipartBoundary() const
{
	return (_multipartBoundary);
}

void HTTP_Request::setParseStatus(ParseStatus status)
{
	_parseStatus = status;
}

HTTP_Request HTTP_Request::getDisplayFriendlyRequest()
{
	HTTP_Request displayFriendlyRequest(*this);
	displayFriendlyRequest._isDisplayFriendlyRequest = true;

	std::string body = displayFriendlyRequest.getBody();
	std::ostringstream newBody;

	if (body.size() > 79)
	{
		newBody << (body.substr(0, 20))
				<< " ... total of " << body.size() << " bytes ... "
				<< body.substr(body.size() - 20, 20);
		displayFriendlyRequest.setBody(newBody.str(), newBody.str().size());
	}

	return (displayFriendlyRequest);
}

bool HTTP_Request::hasHeader(const std::string &fieldName) const
{
	if (_headers.count(fieldName) > 0)
		return (true);
	return (false);
}

void HTTP_Request::dumpToFile(const std::string &filename) const
{
	std::string filename_dumped_to = Utils::dumpToFile(filename, serialize());
	std::cout << "[DEBUG] Request saved/dumped to " << filename_dumped_to << std::endl;
}

int HTTP_Request::_parseRequestLine(std::string line)
{
	/*
	 *	getline removes '\n' (LF), so,
	 *	only check and remove '\r' (CR).
	 */
	if (!removeLastPortion(line, CR))
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	int part = 1;
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	std::string subString;
	while ((end = line.find(" ", start)) != std::string::npos &&
		   part < 3)
	{
		subString = line.substr(start, end - start);
		start = end + 1;
		if (part == 1)
		{
			if (!_parseMethod(subString))
			{
				_parseStatus = HTTP_Request::BAD_REQUEST;
				return (FAILURE);
			}
		}
		else if (part == 2)
		{
			if (!_parseURL(subString))
			{
				_parseStatus = HTTP_Request::BAD_REQUEST;
				return (FAILURE);
			}
		}
		part++;
	}

	if (part == 3)
	{
		subString = line.substr(start, line.size() - start);
		if (!_parseVersion(subString))
		{
			_parseStatus = HTTP_Request::HTTP_VERSION_NOT_SUPPORTED;
			return (FAILURE);
		}
	}
	else
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	/*	request line parsed succesfully
	 *	this line is mandatory to be present in the request,
	 *	therefore, count it in the required headers
	 */
	_headersRequiredCount++;
	return (SUCCESS);
}

int HTTP_Request::_parseMethod(std::string method)
{
	if (method == HTTP_Method::GET ||
		method == HTTP_Method::HEAD ||
		method == HTTP_Method::POST ||
		method == HTTP_Method::DELETE)
	{
		this->_method = method;
		return (SUCCESS);
	}
	return (FAILURE);
}

int HTTP_Request::_parseURL(std::string url)
{
	if (!_URLIsValid(url))
		return (FAILURE);
	this->_url = url;
	return (SUCCESS);
}

int HTTP_Request::_parseVersion(std::string version)
{
	if (version == HTTP_Version::v1_0 ||
		version == HTTP_Version::v1_1)
	{
		this->_version = version;
		return (SUCCESS);
	}
	return (FAILURE);
}

int HTTP_Request::_URLIsValid(const std::string &url)
{
	return (SUCCESS);

	if (url.empty())
		return (FAILURE);

	size_t pos = 0;

	// PATH, QUERY, AND FRAGMENT COMPONENTS
	// RFC 3986 Section 3.3 (Path), Section 3.4 (Query), Section 3.5 (Fragment)
	// Characters must match strict unreserved/reserved specifications or %HH
	while (pos < url.length())
	{
		if (url[pos] == '%')
		{
			if (!Utils::isValidPercentEncoded(url, pos))
				return (FAILURE); // malformed percent escape triplet
			pos += 3;
		}
		else
		{
			if (!Utils::isValidUriChar(url[pos]))
				return (FAILURE); // illegal character present in url string
			pos++;
		}
	}

	return (SUCCESS);
}

int HTTP_Request::_parseHeaderLine(std::string line)
{
	/*
	 *	getline removes '\n' (LF), so,
	 *	only check and remove '\r' (CR).
	 */
	if (!removeLastPortion(line, CR))
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	std::string::size_type pos = line.find(":", 0);
	if (pos == std::string::npos)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	// 1. Extract and store in variables (Fixes compilation error)
	std::string fieldName = line.substr(0, pos);
	std::string value = line.substr(pos + 1);

	// 2. Trim both (Ensure " \t" is included for the key)
	Utils::trim(fieldName, DISALLOWED_CHARS_IN_FIELD_VALUE);
	Utils::trim(value, DISALLOWED_CHARS_IN_FIELD_VALUE);

	if (!_fieldNameIsValid(fieldName) || !_headerValueIsValid(value) ||
		_fieldNameAlreadyProcessed(fieldName) || _fieldNameIsSecurityRisk(fieldName))
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	// 3. Normalize for display/storage
	std::string displayKey = capitaliseFirstLetters(fieldName);

	// 4. Robust check for Content-Length (Fixes the Hyphen bug)
	// Using a lowercase check is safer than relying on capitalization
	std::string lowerKey = fieldName;
	for (size_t i = 0; i < lowerKey.length(); ++i)
		lowerKey[i] = tolower(lowerKey[i]);

	if (lowerKey == "content-length")
	{
		size_t value_size_t;
		if (toNumber(value, value_size_t, FORBID_NEGATIVES))
		{
			_headers[HTTP_FieldName::CONTENT_LENGTH] = toString(value_size_t);
		}
		else
		{
			_parseStatus = HTTP_Request::BAD_REQUEST;
			return (FAILURE);
		}
	}
	else
	{
		_headers[displayKey] = value;

		if (displayKey == HTTP_FieldName::CONTENT_TYPE &&
			value.find("multipart/form-data") != std::string::npos)
			_isMultipartRequest = true;
	}

	_countHeaderIfRequired(displayKey);
	return (SUCCESS);
}

// this function expects no CRLF at the end of the last header line
// the last CRLF should be priorly removed!
HTTP_Headers HTTP_Request::_parseMultipartHeaders(const std::string &multipartHeadersStr)
{
	std::vector<std::string> lines = Utils::split(multipartHeadersStr, CRLF);
	
	HTTP_Headers multipartHeaders;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		std::string::size_type colon_pos = lines[i].find(":", 0);
		if (colon_pos == std::string::npos)
			throw std::runtime_error("Malformed multipart header \"" + lines[i] + "\"");

		// 1. Extract and store in variables (Fixes compilation error)
		std::string fieldName = lines[i].substr(0, colon_pos);
		std::string value = lines[i].substr(colon_pos + 1);

		// 2. Trim both (Ensure " \t" is included for the key)
		Utils::trim(fieldName, DISALLOWED_CHARS_IN_FIELD_VALUE);
		Utils::trim(value, DISALLOWED_CHARS_IN_FIELD_VALUE);

		if (!_fieldNameIsValid(fieldName) || !_headerValueIsValid(value))
			throw std::runtime_error("Invalid field name or value in multipart header \"" + lines[i] + "\"");

		// 3. Normalize for display/storage
		std::string displayKey = capitaliseFirstLetters(fieldName);
		multipartHeaders[displayKey] = value;
	}

	return (multipartHeaders);
}

int HTTP_Request::populateMultipartVars()
{
	std::string value = _headers[HTTP_FieldName::CONTENT_TYPE];
	_multipartBoundary = _extractFromValue("boundary=", value);

	size_t boundaryCount = Utils::countOccurrence(_body, _multipartBoundary);
	if (boundaryCount == 1)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}
	else if (boundaryCount > 2)
	{
		_parseStatus = HTTP_Request::NOT_IMPLEMENTED;
		return (FAILURE);
	}

	// find first boundary
	std::string delimiter = "--" + _multipartBoundary;
	size_t part_start = _body.find(delimiter);
	if (part_start == std::string::npos)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	// move past first boundary and \r\n
	part_start = _body.find(CRLF, part_start);
	if (part_start == std::string::npos)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}
	part_start += 2;

	// find end of headers (the blank line)
	size_t headers_end = _body.find(DBL_CRLF, part_start);
	if (headers_end == std::string::npos)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	// parse headers
	std::string headersStr = _body.substr(part_start, headers_end - part_start);
	HTTP_Headers multipartHeaders;
	try {
		multipartHeaders = _parseMultipartHeaders(headersStr);
	} catch (std::exception &e) {
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}

	// extract filename
	if (multipartHeaders.count("Content-Disposition") != 1)
	{
		_parseStatus = HTTP_Request::BAD_REQUEST;
		return (FAILURE);
	}
	std::string values = multipartHeaders["Content-Disposition"];
	_multipartFilename = _extractFromValue("filename=", values);

	// extract file content
	size_t content_start = headers_end + 4; // Skip \r\n\r\n

	// find ending boundary
	std::string end_delimiter = + "--" + _multipartBoundary + "--";
	size_t content_end = _body.find(end_delimiter, content_start);
	if (content_end == std::string::npos)
		std::cout << "[DEBUG] ERROR: could not find ending boundary in multipart request body!" << std::endl;

	if (content_end != std::string::npos)
		_multipartData = _body.substr(content_start, content_end - content_start);
	
	// if the data ends with a CRLF, remove it
	if (Utils::endsWith(_multipartData, CRLF))
		_multipartData = _multipartData.erase(_multipartData.size() - 2);
	
	return (SUCCESS);
}

/* this function can include other headers
 * in the future if needed
 */
int HTTP_Request::_countHeaderIfRequired(std::string fieldName)
{
	if (fieldName == HTTP_FieldName::HOST)
		_headersRequiredCount++;
	return (SUCCESS);
}

/* Defined in RFC 9112, summarized in RFC 9110 */
int HTTP_Request::_fieldNameIsValid(std::string fieldName)
{
	const static std::string allowedChars(ALLOWED_CHARS_IN_FIELD_NAME);

	if (fieldName.empty())
		return (FAILURE);

	for (std::string::size_type i = 0; i < fieldName.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(fieldName[i]);

		if (std::isalnum(c))
			continue;

		if (allowedChars.find(c, 0) == std::string::npos)
			return (FAILURE);
	}
	return (SUCCESS);
}

/* defined in RFC 9112, with semantics summarized in RFC 9110 */
int HTTP_Request::_headerValueIsValid(std::string value)
{
	for (std::string::size_type i = 0; i < value.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(value[i]);

		/* IMPORTANT SECURITY CONCERN:
		 * the below has CR and LF and '\t' characters covered!
		 */
		if (c < 32 || c == 127)
			return (FAILURE);
	}
	return (SUCCESS);
}

int HTTP_Request::_fieldNameAlreadyProcessed(std::string fieldName)
{
	if (_headers.find(fieldName) != _headers.end())
		return (SUCCESS);
	return (FAILURE);
}

/* Disallow CONTENT_LENGTH && TRANSFER_ENCODING headers,
 * both at the same time in the http request to avoid "request smuggling"
 */
int HTTP_Request::_fieldNameIsSecurityRisk(std::string fieldName)
{
	if (fieldName == HTTP_FieldName::TRANSFER_ENCODING &&
		_fieldNameAlreadyProcessed(HTTP_FieldName::CONTENT_LENGTH))
		return (SUCCESS);

	if (fieldName == HTTP_FieldName::CONTENT_LENGTH &&
		_fieldNameAlreadyProcessed(HTTP_FieldName::TRANSFER_ENCODING))
		return (SUCCESS);

	return (FAILURE);
}

std::string HTTP_Request::_extractFromValue(const std::string &prefix, const std::string &value)
{
	std::vector<std::string> values = Utils::split(value, ';');

	for (size_t i = 0; i < values.size(); ++i)
	{
		size_t prefix_pos = values[i].find(prefix);
		if (prefix_pos != std::string::npos)
		{
			std::string value = values[i].substr(prefix_pos + prefix.size());
			Utils::trim(value, DISALLOWED_CHARS_IN_FIELD_VALUE);
			if (value[0] == '"')
				value = Utils::removeQuote(value, '"');
			Utils::trim(value, DISALLOWED_CHARS_IN_FIELD_VALUE);
			return (value);
		}
	}

	return ("");
}

bool HTTP_Request::ready()
{
	return (this->_parseStatus == HTTP_Request::COMPLETE ||
			this->_parseStatus == HTTP_Request::BAD_REQUEST);
}

void HTTP_Request::reset()
{
	_init_class_vars();
}

std::ostream &operator<<(std::ostream &os, const HTTP_Request &hr)
{
	if (hr._headers.size() == 0 && hr._body.size() == 0)
	{
		os << "Empty HTTP_Request object! (was just initialized or all states were reset)" << std::endl;
		return (os);
	}

	if (DEBUG_MODE && !hr._requestLine_completed)
	{
		os << "[DEBUG] HTTP_Request - incomplete header line";
		return (os);
	}

	std::string line_start = "";
	std::string line_ending = CRLF;

	if (hr._isDisplayFriendlyRequest)
	{
		line_start = "|| ";
		line_ending = VISIBLE_CRLF "\n";
	}
	os << line_start << hr._method << " " << hr._url << " " << hr._version;
	os << line_ending;

	if (DEBUG_MODE && !hr._headers_completed)
	{
		os << "[DEBUG] HTTP_Request - incomplete headers";
		return (os);
	}

	HTTP_Headers::const_iterator it;
	for (it = hr._headers.begin(); it != hr._headers.end(); ++it)
	{
		std::string fieldName = it->first;
		std::string value = it->second;

		os << line_start << fieldName << ": " << value << line_ending;
	}

	os << line_start << line_ending;

	if (DEBUG_MODE && !hr._body_completed)
	{
		if (!hr.hasHeader(HTTP_FieldName::TRANSFER_ENCODING) &&
			!hr.hasHeader(HTTP_FieldName::CONTENT_LENGTH))
			os << "[DEGUB] HTTP_Request is headers only (has no body)!";
		else
			os << "[DEBUG] HTTP_Request - incomplete body";
		return (os);
	}

	if (hr._body.size() > 0)
		os << line_start;
	os.write(hr._body.c_str(), hr._body.size());
	return (os);
}

void HTTP_Request::setBody(std::string data, size_t len)
{
	//_body.append(data);
	_body.assign(data);
	(void)len;

	_body_completed = true;
	this->_parseStatus = HTTP_Request::COMPLETE;
}

void HTTP_Request::appendToBody(std::string data, size_t len, bool isFinalAppend)
{
	// Use append so chunked/split data isn't overwritten
	_body.append(data);
	(void)len;

	// CRITICAL for CGI: Keep the length variable in sync!
	// _bodyLen = _body.length();

	if (isFinalAppend)
	{
		_body_completed = true;
		this->_parseStatus = HTTP_Request::COMPLETE; // AI: is this correct here?
	}
}