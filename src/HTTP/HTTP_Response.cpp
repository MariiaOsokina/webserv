/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Response.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/16 16:34:38 by aistok            #+#    #+#             */
/*   Updated: 2026/06/06 20:49:46 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP_Response.hpp"

#include <stddef.h>
#include <sstream>

void HTTP_Response::_init_class_vars()
{
	_status = HTTP_Status::UNSET;
	_version = HTTP_Version::v1_1;

	_headers.clear();
	_addDefaultHeaders();

	_isHEADresponse = false;
	_isCGIGenerated = false;

	_body = "";

	_cgiPath = "";
	_scriptPath = "";
}

void HTTP_Response::_set_class_vars(const HTTP_Response &other)
{
	_status = other._status;
	_version = other._version;

	_headers = other._headers;

	_isHEADresponse = other._isHEADresponse;
	_isCGIGenerated = other._isCGIGenerated;

	_body = other._body;

	_cgiPath = other._cgiPath;
	_scriptPath = other._scriptPath;
}

HTTP_Response::HTTP_Response()
{
	_init_class_vars();
	_addDefaultHeaders();
}

HTTP_Response::HTTP_Response(const HTTP_StatusPair &status)
{
	_init_class_vars();

	setStatus(status);

	_addDefaultHeaders();
}

HTTP_Response::HTTP_Response(
	const HTTP_StatusPair &status, std::string textContent)
{
	_init_class_vars();

	setStatus(status);
	setContent(textContent);

	_addDefaultHeaders();
}

HTTP_Response::HTTP_Response(const HTTP_Response &other)
{
	if (this != &other)
		_set_class_vars(other);
}

HTTP_Response &HTTP_Response::operator=(const HTTP_Response &other)
{
	if (this != &other)
		_set_class_vars(other);

	return (*this);
}

HTTP_Response::~HTTP_Response() {}

std::map<std::string, std::string> &HTTP_Response::getHeaders()
{
	return (_headers);
}

void HTTP_Response::setStatus(const HTTP_StatusPair &status)
{
	_status = status;
}

std::string HTTP_Response::serialize() const
{
	std::ostringstream oss(std::ios::binary);
	oss << *this;
	return (oss.str());
}

void HTTP_Response::serializeHeadersInto(std::string &out) const
{
	size_t header_estimate = 256 + _headers.size() * 80;
	out.clear();
	out.reserve(header_estimate);

	out.append(_version);
	out.append(" ");
	out.append(::toString(_status.code));
	out.append(" ");
	out.append(_status.text);
	out.append(CRLF);

	std::map<std::string, std::string>::const_iterator it;
	for (it = _headers.begin(); it != _headers.end(); ++it)
	{
		out.append(it->first);
		out.append(": ");
		out.append(it->second);
		out.append(CRLF);
	}
	out.append(CRLF);
}

void HTTP_Response::serializeInto(std::string &out)
{
	// Estimate so we avoid most reallocations. The 256 covers status
	// line + a generous header average; the body dominates anyway.
	size_t header_estimate = 256 + _headers.size() * 80;
	out.clear();
	out.reserve(header_estimate + _body.size());

	out.append(_version);
	out.append(" ");
	out.append(::toString(_status.code));
	out.append(" ");
	out.append(_status.text);
	out.append(CRLF);

	std::map<std::string, std::string>::const_iterator it;
	for (it = _headers.begin(); it != _headers.end(); ++it)
	{
		out.append(it->first);
		out.append(": ");
		out.append(it->second);
		out.append(CRLF);
	}
	out.append(CRLF);

	// Move the body in (no copy of the 100+ MB blob): swap into a
	// scratch string and append from it, then let it die.
	if (!_body.empty())
	{
		std::string body_tmp;
		body_tmp.swap(_body);
		out.append(body_tmp.data(), body_tmp.size());
	}
}

void HTTP_Response::setContent(const std::string &text)
{
	if (!_isHEADresponse)
	{
		_body = text;
		_headers[HTTP_FieldName::CONTENT_LENGTH] = ::toString(text.length());
	}
}

void HTTP_Response::appendToContent(const std::string &data)
{
	if (!_isHEADresponse)
	{
		_body += data;
		_headers[HTTP_FieldName::CONTENT_LENGTH] = ::toString(data.length());
	}
}

void HTTP_Response::setHeadersOnly(const bool value)
{
	_isHEADresponse = value;
}

bool HTTP_Response::isHeadersOnly()
{
	return (_isHEADresponse);
}

void HTTP_Response::setCGIGenerated(const bool value)
{
	_isCGIGenerated = value;
}

bool HTTP_Response::isCGIGenerated()
{
	return (_isCGIGenerated);
}

void HTTP_Response::swapBody(std::string &dst)
{
	_body.swap(dst);
	_headers[HTTP_FieldName::CONTENT_LENGTH] = ::toString(_body.length());
}

void HTTP_Response::clearBody()
{
	std::string().swap(_body);
}

void HTTP_Response::reset()
{
	_init_class_vars();
}

void HTTP_Response::_addDefaultHeaders(bool addDebugHeaders)
{
	_addServerDate();
	_addServerNameHeader();
	if (addDebugHeaders)
		_addDegubHeaders();
}

void HTTP_Response::_addServerNameHeader()
{
	_headers[HTTP_FieldName::SERVER_NAME] = WEBSERV_NAME;
}

void HTTP_Response::_addServerDate()
{
	_headers[HTTP_FieldName::DATE] = Utils::getHttpDate();
}

void HTTP_Response::_addDegubHeaders()
{
	// tell the browsers to not cache the responses
	// this way the browser will initiate a request every time,
	// even when the files were already sent (ex static files that do not change)
	_headers["Cache-Control"] = "no-store, no-cache, must-revalidate, max-age=0";
	_headers["Pragma"] = "no-cache";
	_headers["Expires"] = "0";
}

std::ostream &operator<<(std::ostream &os, const HTTP_Response &hResp)
{
	os << hResp._version << " " << hResp._status.code << " " << hResp._status.text << CRLF;

	std::map<std::string, std::string>::const_iterator it;
	for (it = hResp._headers.begin(); it != hResp._headers.end(); ++it)
	{
		std::string fieldName = it->first;
		std::string value = it->second;

		os << fieldName << ": " << value << CRLF;
	}

	os << CRLF;

	if (hResp._body.size() > 0)
		os.write(hResp._body.c_str(), hResp._body.size());

	return (os);
}

void HTTP_Response::setCgiPath(const std::string &path)
{
	_cgiPath = path;
}

std::string HTTP_Response::getCgiPath() const
{
	return _cgiPath;
}

void HTTP_Response::setScriptPath(const std::string &path)
{
	_scriptPath = path;
}

std::string HTTP_Response::getScriptPath() const
{
	return _scriptPath;
}

void HTTP_Response::dumpToFile(const std::string &filename) const
{
	std::string filename_dumped_to = Utils::dumpToFile(filename, serialize());
	std::cout << "[DEBUG] Response saved/dumped to " << filename_dumped_to << std::endl;
}
