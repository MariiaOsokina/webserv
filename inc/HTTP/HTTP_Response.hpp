/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Response.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/16 16:34:38 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 09:02:35 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <iostream>
#include <sstream>
#include <map>

#include "HTTP_Version.hpp"
#include "HTTP_Status.hpp"
#include "HTTP_Method.hpp"
#include "HTTP_FieldName.hpp"
#include "Utils.hpp"

class HTTP_Response
{
public:
	HTTP_Response();
	HTTP_Response(const HTTP_StatusPair &status);
	HTTP_Response(const HTTP_StatusPair &status, std::string textContent);
	HTTP_Response(const HTTP_Response &other);
	HTTP_Response &operator=(const HTTP_Response &other);
	~HTTP_Response();

	std::map<std::string, std::string> &getHeaders();

	void setStatus(const HTTP_StatusPair &status);

	std::string serialize() const;
	// Build the wire bytes directly into `out` and move the body out
	// of this response (leaving _body empty). One body copy in total,
	// instead of the 2–3 ostringstream-based serialize() incurs. Used
	// on the hot path; large CGI responses depend on this to fit in RAM.
	void serializeInto(std::string &out);
	// Status line + headers + CRLF, body excluded. Used by the CGI
	// streaming path so the headers can go on the wire before the
	// CGI's body has even finished arriving.
	void serializeHeadersInto(std::string &out) const;
	void setContent(const std::string &text);
	void appendToContent(const std::string &data);

	void setHeadersOnly(const bool value);
	bool isHeadersOnly();

	void setCGIGenerated(const bool value);
	bool isCGIGenerated();
	// O(1) ownership transfer of the body. Used during CGI output
	// parsing (absorb the body without a copy) and after serialize()
	// (free the body once it has been baked into the raw response).
	// Without these, a 100 MB CGI body lived simultaneously in the CGI
	// buffer, the Response, and the Connection's _rawResponse — enough
	// to OOM-kill the server under concurrent uploads.
	void swapBody(std::string &dst);
	void clearBody();
	void reset();

	void setCgiPath(const std::string &path);
	std::string getCgiPath() const;

	void setScriptPath(const std::string &path);
	std::string getScriptPath() const;

	void dumpToFile(const std::string &filename) const;

	// friend is needed for the operator<< to be able to access
	// the status and version private variables
	friend std::ostream &operator<<(std::ostream &os, const HTTP_Response &hResp);

protected:
	// ...

private:
	HTTP_StatusPair _status;
	std::string _version;

	std::map<std::string, std::string> _headers;

	bool _isHEADresponse;
	bool _isCGIGenerated;
	std::string _body;

	std::string _cgiPath;
	std::string _scriptPath;

	void _init_class_vars();
	void _set_class_vars(const HTTP_Response &other);

	void _addDefaultHeaders(bool addDebugHeaders = false);
	void _addServerNameHeader();
	void _addServerDate();
	void _addDegubHeaders();
};

#endif // HTTP_RESPONSE_HPP
