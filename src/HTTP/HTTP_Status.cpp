/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Status.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/12 14:39:13 by aistok            #+#    #+#             */
/*   Updated: 2026/06/06 20:49:59 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP_Status.hpp"

const HTTP_StatusPair HTTP_Status::_table[] = {
	{0, "Unset"},

	{100, "Continue"},

	{200, "OK"},
	{201, "Created"},
	{202, "Accepted"},
	{204, "No Content"},

	{301, "Moved Permanently"},
	{302, "Found"},
	{303, "See Other"},
	{304, "Not Modified"},
	{307, "Internal Redirect"},
	{308, "Permanent Redirect"},

	{400, "Bad Request"},
	{401, "Unauthorized"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{405, "Method Not Allowed"},
	{408, "Request Timeout"},
	{409, "Conflict"},
	{411, "Length Required"},
	{413, "Content Too Large"},
	{418, "I'm a teapot"},
	{422, "Unprocessable Entity"},
	{431, "Request Header Fields Too Large"},

	{500, "Internal Server Error"},
	{501, "Not Implemented"},
	{502, "Bad Gateway"},
	{503, "Service Unavailable"},
	{504, "Gateway Timeout"},
	{505, "HTTP Version Not Supported"},
};

const int HTTP_Status::_count =
	sizeof(HTTP_Status::_table) / sizeof(HTTP_StatusPair);

const HTTP_StatusPair HTTP_Status::UNSET = HTTP_Status::_table[0];
const HTTP_StatusPair HTTP_Status::s0 = HTTP_Status::UNSET;

/* 1xx Informational */
const HTTP_StatusPair HTTP_Status::CONTINUE = HTTP_Status::_table[1];
const HTTP_StatusPair HTTP_Status::s100 = HTTP_Status::CONTINUE;

/* 2xx Success */
const HTTP_StatusPair HTTP_Status::OK = HTTP_Status::_table[2];
const HTTP_StatusPair HTTP_Status::s200 = HTTP_Status::OK;

const HTTP_StatusPair HTTP_Status::CREATED = HTTP_Status::_table[3];
const HTTP_StatusPair HTTP_Status::s201 = HTTP_Status::CREATED;

const HTTP_StatusPair HTTP_Status::ACCEPTED = HTTP_Status::_table[4];
const HTTP_StatusPair HTTP_Status::s202 = HTTP_Status::ACCEPTED;

const HTTP_StatusPair HTTP_Status::NO_CONTENT = HTTP_Status::_table[5];
const HTTP_StatusPair HTTP_Status::s204 = HTTP_Status::NO_CONTENT;

/* 3xx Redirections */
const HTTP_StatusPair HTTP_Status::MOVED_PERMANENTLY = HTTP_Status::_table[6];
const HTTP_StatusPair HTTP_Status::s301 = HTTP_Status::MOVED_PERMANENTLY;

const HTTP_StatusPair HTTP_Status::FOUND = HTTP_Status::_table[7];
const HTTP_StatusPair HTTP_Status::s302 = HTTP_Status::FOUND;

const HTTP_StatusPair HTTP_Status::SEE_OTHER = HTTP_Status::_table[8];
const HTTP_StatusPair HTTP_Status::s303 = HTTP_Status::SEE_OTHER;

const HTTP_StatusPair HTTP_Status::NOT_MODIFIED = HTTP_Status::_table[9];
const HTTP_StatusPair HTTP_Status::s304 = HTTP_Status::NOT_MODIFIED;

const HTTP_StatusPair HTTP_Status::INTERNAL_REDIRECT = HTTP_Status::_table[10];
const HTTP_StatusPair HTTP_Status::s307 = HTTP_Status::INTERNAL_REDIRECT;

const HTTP_StatusPair HTTP_Status::PERMANENT_REDIRECT = HTTP_Status::_table[11];
const HTTP_StatusPair HTTP_Status::s308 = HTTP_Status::PERMANENT_REDIRECT;

/* 4xx Client Errors */
const HTTP_StatusPair HTTP_Status::BAD_REQUEST = HTTP_Status::_table[12];
const HTTP_StatusPair HTTP_Status::s400 = HTTP_Status::BAD_REQUEST;

const HTTP_StatusPair HTTP_Status::UNAUTHORIZED = HTTP_Status::_table[13];
const HTTP_StatusPair HTTP_Status::s401 = HTTP_Status::UNAUTHORIZED;

const HTTP_StatusPair HTTP_Status::FORBIDDEN = HTTP_Status::_table[14];
const HTTP_StatusPair HTTP_Status::s403 = HTTP_Status::FORBIDDEN;

const HTTP_StatusPair HTTP_Status::NOT_FOUND = HTTP_Status::_table[15];
const HTTP_StatusPair HTTP_Status::s404 = HTTP_Status::NOT_FOUND;

const HTTP_StatusPair HTTP_Status::METHOD_NOT_ALLOWED = HTTP_Status::_table[16];
const HTTP_StatusPair HTTP_Status::s405 = HTTP_Status::METHOD_NOT_ALLOWED;

const HTTP_StatusPair HTTP_Status::REQUEST_TIMEOUT = HTTP_Status::_table[17];
const HTTP_StatusPair HTTP_Status::s408 = HTTP_Status::REQUEST_TIMEOUT;

const HTTP_StatusPair HTTP_Status::CONFLICT = HTTP_Status::_table[18];
const HTTP_StatusPair HTTP_Status::s409 = HTTP_Status::CONFLICT;

const HTTP_StatusPair HTTP_Status::LENGTH_REQUIRED = HTTP_Status::_table[19];
const HTTP_StatusPair HTTP_Status::s411 = HTTP_Status::LENGTH_REQUIRED;

const HTTP_StatusPair HTTP_Status::CONTENT_TOO_LARGE = HTTP_Status::_table[20];
const HTTP_StatusPair HTTP_Status::s413 = HTTP_Status::CONTENT_TOO_LARGE;

const HTTP_StatusPair HTTP_Status::IM_A_TEAPOT = HTTP_Status::_table[21];
const HTTP_StatusPair HTTP_Status::s418 = HTTP_Status::IM_A_TEAPOT;

const HTTP_StatusPair HTTP_Status::UNPROCESSABLE_CONTENT = HTTP_Status::_table[22];
const HTTP_StatusPair HTTP_Status::s422 = HTTP_Status::UNPROCESSABLE_CONTENT;

const HTTP_StatusPair HTTP_Status::REQUEST_HEADER_FIELDS_TOO_LARGE = HTTP_Status::_table[23];
const HTTP_StatusPair HTTP_Status::s431 = HTTP_Status::REQUEST_HEADER_FIELDS_TOO_LARGE;

/* 5xx Server Errors */
const HTTP_StatusPair HTTP_Status::INTERNAL_SERVER_ERROR = HTTP_Status::_table[24];
const HTTP_StatusPair HTTP_Status::s500 = HTTP_Status::INTERNAL_SERVER_ERROR;

const HTTP_StatusPair HTTP_Status::NOT_IMPLEMENTED = HTTP_Status::_table[25];
const HTTP_StatusPair HTTP_Status::s501 = HTTP_Status::NOT_IMPLEMENTED;

const HTTP_StatusPair HTTP_Status::BAD_GATEWAY = HTTP_Status::_table[26];
const HTTP_StatusPair HTTP_Status::s502 = HTTP_Status::BAD_GATEWAY;

const HTTP_StatusPair HTTP_Status::SERVICE_UNAVAILABLE = HTTP_Status::_table[27];
const HTTP_StatusPair HTTP_Status::s503 = HTTP_Status::SERVICE_UNAVAILABLE;

const HTTP_StatusPair HTTP_Status::GATEWAY_TIMEOUT = HTTP_Status::_table[28];
const HTTP_StatusPair HTTP_Status::s504 = HTTP_Status::GATEWAY_TIMEOUT;

const HTTP_StatusPair HTTP_Status::HTTP_VERSION_NOT_SUPPORTED = HTTP_Status::_table[29];
const HTTP_StatusPair HTTP_Status::s505 = HTTP_Status::HTTP_VERSION_NOT_SUPPORTED;

const HTTP_StatusPair &HTTP_Status::fromCode(int code)
{
	for (int i = 0; i < _count; ++i)
	{
		if (_table[i].code == code)
			return _table[i];
	}
	return _table[0];
}

const HTTP_StatusPair *HTTP_Status::all()
{
	return _table;
}

int HTTP_Status::count()
{
	return _count;
}

bool HTTP_StatusPair::operator==(const HTTP_StatusPair &other) const
{
	return (this->code == other.code);
}
