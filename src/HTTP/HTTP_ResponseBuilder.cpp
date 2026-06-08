/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_ResponseBuilder.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/20 10:48:39 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:53:19 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP_ResponseBuilder.hpp"

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <cstdio>

HTTP_ResponseBuilder::Exception::Exception(const HTTP_StatusPair &status, const std::string &msg)
	: _status(status), _message(msg) {}

HTTP_ResponseBuilder::Exception::~Exception() throw() {}

const char *HTTP_ResponseBuilder::Exception::what() const throw()
{
	return _message.c_str();
}

HTTP_StatusPair HTTP_ResponseBuilder::Exception::getStatus() const
{
	return _status;
}

HTTP_ResponseBuilder::HTTP_ResponseBuilder() {}

HTTP_ResponseBuilder::HTTP_ResponseBuilder(const ServerConfig &sc)
{
	_serverConfig = sc;
}

HTTP_ResponseBuilder::~HTTP_ResponseBuilder()
{
}

void HTTP_ResponseBuilder::build(HTTP_Response &response, HTTP_Request &request)
{
	DebugLogger(std::cout)("++ GOT REQUEST ++\n");
	DebugLogger(std::cout)(request.getDisplayFriendlyRequest());
	DebugLogger(std::cout)("++ REQUEST FIN ++\n");

	int parseStatus = request.getParseStatus();

	if (parseStatus == HTTP_Request::INCOMPLETE)
		return;

	const std::string method = request.getMethod();
	if (method == HTTP_Method::HEAD)
		response.setHeadersOnly(true);

	if (parseStatus >= 400)
	{
		setResponse(response, HTTP_Status::fromCode(parseStatus));
		return;
	}

	try
	{
		_location = locationGetBestMatch(request);
	}
	catch (HTTP_ResponseBuilder::Exception &e)
	{
		if (e.getStatus() == HTTP_Status::FOUND)
		{
			DebugLogger(std::cout)("[DEBUG] HTTP_ResponseBuilder::build - location, canonicalization redirect!")('\n');
			HTTP_ResponseBuilder::setResponseRedirect(response, HTTP_Status::FOUND.code, e.what());
			return;
		}

		// the only other possible exception at the moment is HTTP_Status::NOT_FOUND
		DebugLogger(std::cout)("[DEBUG] HTTP_ResponseBuilder::build - locationGetBestMatch:")('\n')("        ")(e.getStatus().text)(": ")(e.what())('\n');
		setResponse(response, HTTP_Status::NOT_FOUND);
		return;
	}

	if (_location.redirect_code > 0 || !_location.redirect_url.empty())
	{
		int statusCode = _location.redirect_code;
		std::string url = _location.redirect_url;

		if (statusCode > 0 && !url.empty())
		{
			response.setStatus(HTTP_Status::fromCode(statusCode));
			response.getHeaders()[HTTP_FieldName::LOCATION] = url;
			response.setContent("");
			return;
		}
		else if (statusCode == 0 && !url.empty())
		{
			response.setStatus(HTTP_Status::MOVED_PERMANENTLY);
			response.getHeaders()[HTTP_FieldName::LOCATION] = url;
			response.setContent("");
			return;
		}

		response.setStatus(HTTP_Status::fromCode(statusCode));
		response.setContent(ErrorPages::getContent(_serverConfig, HTTP_Status::fromCode(statusCode)));
		return;
	}

	if (!locationHasMethod(_location, method))
	{
		setResponse(response, HTTP_Status::METHOD_NOT_ALLOWED);
		return;
	}

	// Location-level client_max_body_size enforcement. The Connection
	// only knows the server-level limit when it parses Content-Length,
	// so a tighter location limit (e.g. /post_body { client_max_body_size 100; })
	// would otherwise be ignored. Apply it here once the location is known.
	{
		size_t loc_limit = _location.client_max_body_size > 0
							   ? _location.client_max_body_size
							   : _serverConfig.client_max_body_size;
		if (loc_limit > 0 && request.getBody().length() > loc_limit)
		{
			setResponse(response, HTTP_Status::CONTENT_TOO_LARGE);
			return;
		}
	}

	try
	{
		_pathOnServer = translateUriToPath(request);
		DebugLogger(std::cout)("[DEBUG] pathOnServer: ")(_pathOnServer)('\n');
	}
	catch (HTTP_ResponseBuilder::Exception &e)
	{
		DebugLogger(std::cout)("[DEBUG] HTTP_ResponseBuilder::translateUriToPath : ")(e.what())('\n');
		setResponse(response, e.getStatus());
		return;
	}

	_pathType = getPathType(_pathOnServer);

	if ((_pathType == PATH_FILE || _pathType == PATH_NONE) && (method == HTTP_Method::GET || method == HTTP_Method::POST))
	{
		std::string ext = Utils::getExtension(_pathOnServer);
		DebugLogger(std::cout)("[DEBUG] Checking CGI for Path: ")(_pathOnServer)('\n');
		DebugLogger(std::cout)("[DEBUG] Ext extracted: [")(ext)("]\n");
		DebugLogger(std::cout)("[DEBUG] Is ext in map? ")(CGILauncher::forCGIResponse(_pathOnServer, _location.cgi_extensions) ? "YES" : "NO")('\n');
		if (CGILauncher::forCGIResponse(_pathOnServer, _location.cgi_extensions))
		{
			std::string cgi_path = CGILauncher::getCGIPath(_pathOnServer, _location.cgi_extensions);

			response.setCGIGenerated(true);
			response.setCgiPath(cgi_path);
			response.setScriptPath(_pathOnServer);
		}
	}

	// for POST requests, we need to check request.upload_path + filename,
	// and, since filename will only be known later, we handle this in the
	// POST response building
	if ((method != HTTP_Method::POST) &&
		(_pathType == PATH_NONE && !response.isCGIGenerated()))
	{
		setResponse(response, HTTP_Status::NOT_FOUND);
		return;
	}

	DebugLogger(std::cout)("[DEBUG] _serverConfig.client_max_body_size --> ")(_serverConfig.client_max_body_size)('\n');
	DebugLogger(std::cout)("[DEBUG] _location.client_max_body_size --> ")(_location.client_max_body_size)('\n');

	if (response.isCGIGenerated())
	{
		DebugLogger(std::cout)("[DEBUG] CGI Will be used!\n");
		return;
	}
	else if (method == HTTP_Method::GET || method == HTTP_Method::HEAD)
	{
		DebugLogger(std::cout)("[DEBUG] Handling GET request! (method = ")(method)(")\n");
		build_response_for_GET_or_HEAD(response, request);
	}
	else if (method == HTTP_Method::POST)
	{
		DebugLogger(std::cout)("[DEBUG] Handling POST request!\n");
		build_response_for_POST(response, request);
	}
	else if (method == HTTP_Method::DELETE)
	{
		DebugLogger(std::cout)("[DEBUG] Handling DELETE request!\n");
		build_response_for_DELETE(response, request);
	}
	else
		setResponse(response, HTTP_Status::NOT_IMPLEMENTED);
}

void HTTP_ResponseBuilder::setResponse(HTTP_Response &response, const HTTP_StatusPair &status)
{
	HTTP_ResponseBuilder::setResponse(response, status, _serverConfig);
}

void HTTP_ResponseBuilder::setResponse(HTTP_Response &response, const HTTP_StatusPair &status, const ServerConfig &sc)
{
	response.setStatus(status);
	if (!response.isHeadersOnly())
	{
		response.setContent(ErrorPages::getContent(sc, status));
	}
}

void HTTP_ResponseBuilder::setResponseRedirect(HTTP_Response &response, const int statusCode, const std::string &url)
{
	response.setStatus(HTTP_Status::fromCode(statusCode));
	response.getHeaders()[HTTP_FieldName::LOCATION] = url;
	response.setContent("");
}

bool HTTP_ResponseBuilder::locationHasMethod(const LocationConfig &location, const std::string &method)
{
	std::vector<std::string>::const_iterator method_it = location.allowed_methods.begin();
	for (; method_it != location.allowed_methods.end(); ++method_it)
	{
		if (*method_it == method)
			return (true);
	}
	return (false);
}

void HTTP_ResponseBuilder::build_response_for_GET_or_HEAD(HTTP_Response &response, const HTTP_Request &request)
{
	// _pathType == PATH_NONE is handled before the function call

	if (_pathType == PATH_FILE)
	{
		if (!Utils::isReadable(_pathOnServer))
			setResponse(response, HTTP_Status::FORBIDDEN);

		else
		{
			response.setStatus(HTTP_Status::OK);
			if (!response.isHeadersOnly())
				response.setContent(Utils::getFileContent(_pathOnServer));
		}
		return;
	}
	else if (_pathType == PATH_DIRECTORY)
	{
		std::string directoryURL = request.getURLWithoutParams();
		char lastChar = *(directoryURL.rbegin());
		if (lastChar != '/')
		{
			// URL normalization or path canonicalization redirect
			// the directory exists, but the client did not request
			// it properly, there was a missing '/' at the end
			HTTP_ResponseBuilder::setResponseRedirect(
				response,
				HTTP_Status::FOUND.code,
				directoryURL + "/");
			DebugLogger(std::cout)("[DEBUG] +++ URL normalization\n");
			return;
		}

		std::string theIndexFile;

		if (!_location.index.empty())
			theIndexFile = _location.index;
		else if (!_serverConfig.index.empty())
			theIndexFile = _serverConfig.index;

		if (!theIndexFile.empty())
		{
			std::string indexOnServer = Utils::joinPath(_pathOnServer, theIndexFile);
			PathType indexType = getPathType(indexOnServer);

			if (indexType == PATH_FILE)
			{
				try
				{
					if (!response.isHeadersOnly())
						response.setContent(Utils::getFileContent(indexOnServer));
					response.setStatus(HTTP_Status::OK);
				}
				catch (std::exception &e)
				{
					DebugLogger(std::cout)("[DEBUG] Unable to open file ")(indexOnServer)('\n');
					setResponse(response, HTTP_Status::FORBIDDEN);
				}
				return;
			}
			else if ((indexType == PATH_NONE) && // no index on filesystem
					 !_location.autoindex)		 // autoindex is off!
			{
				setResponse(response, HTTP_Status::NOT_FOUND);
				return;
			}
		}

		if (_location.autoindex && !Utils::isReadable(_pathOnServer))
		{
			setResponse(response, HTTP_Status::FORBIDDEN);
			return;
		}

		// dir exists -> _pathOnServer and we have read access to the directory!
		if (response.isHeadersOnly())
			setResponse(response, HTTP_Status::OK);
		else
		{
			// if not HEAD request, list directory content
			response.setStatus(HTTP_Status::OK);

			std::string htmlDirectories =
				DirectoriesToHTML::generate(
					Utils::getDirectoryList(_pathOnServer),
					request.getURLWithoutParams(),
					locationHasMethod(_location, HTTP_Method::DELETE));

			response.setContent(htmlDirectories);
			return;
		}
	}

	// execution should not reach here
	setResponse(response, HTTP_Status::INTERNAL_SERVER_ERROR);
	return;
}

void HTTP_ResponseBuilder::build_response_for_POST(
	HTTP_Response &response,
	HTTP_Request &request)
{
	std::string filenameOnServer;
	std::string dataToWrite;
	std::string errorMsg = "";

	if (_location.upload_path.empty())
		errorMsg = "_location.upload_path is empty!";

	else if (getPathType(_location.upload_path) != PATH_DIRECTORY)
		errorMsg = _location.upload_path + " has to be a directory!";

	else if (!Utils::isWritable(_location.upload_path))
		errorMsg = _location.upload_path + " has no write access!";

	if (!errorMsg.empty())
	{
		DebugLogger(std::cout)("[DEBUG] ERROR: ")(errorMsg)('\n');
		setResponse(response, HTTP_Status::INTERNAL_SERVER_ERROR);
		return;
	}

	if (!request.isMultipartRequest())
	{
		DebugLogger(std::cout)("[DEBUG] Request is NOT multipart!\n");
		filenameOnServer = Utils::joinPath(_location.upload_path, DEFAULT_UPLOAD_FILENAME);
		dataToWrite = Utils::urlDecode(request.getBody());
	}
	else
	{
		DebugLogger(std::cout)("[DEBUG] Request IS multipart!\n");
		if (request.populateMultipartVars() == FAILURE)
		{
			DebugLogger(std::cout)("[DEBUG] ERROR reading multipart request!\n");
			setResponse(response, HTTP_Status::BAD_REQUEST);
			return;
		}

		DebugLogger(std::cout)("[DEBUG] Boundary: ")(request.getMultipartBoundary())('\n');
		DebugLogger(std::cout)("[DEBUG] Filename: ")(request.getMultipartFilename())('\n');
		DebugLogger(std::cout)("[DEBUG] Data start>>>")(request.getMultipartData())("<<<Data fin")('\n');

		filenameOnServer = Utils::joinPath(_location.upload_path, request.getMultipartFilename());

		if (filenameOnServer == _location.upload_path)
			filenameOnServer = Utils::joinPath(_location.upload_path, DEFAULT_UPLOAD_FILENAME);

		dataToWrite = request.getMultipartData();
	}

	filenameOnServer = Utils::getNextAvailableFilename(filenameOnServer);

	std::ofstream fileOnServer(filenameOnServer.c_str(), std::ios::binary);
	if (!fileOnServer.is_open())
	{
		DebugLogger(std::cout)("[DEBUG] Upload error: Could not open file ")(filenameOnServer)('\n');
		setResponse(response, HTTP_Status::INTERNAL_SERVER_ERROR);
		return;
	}

	fileOnServer.write(dataToWrite.c_str(), dataToWrite.size());
	fileOnServer.close();

	DebugLogger(std::cout)("[INFO] Uploaded file saved to ")(filenameOnServer)(" (")(dataToWrite.size())(" bytes)\n");

	response.setStatus(HTTP_Status::CREATED);
	response.setContent("File upload successfull!");
	return;
}

void HTTP_ResponseBuilder::build_response_for_DELETE(
	HTTP_Response &response,
	const HTTP_Request &request)
{
	(void)request;
	if (std::remove(_pathOnServer.c_str()) == 0)
		setResponse(response, HTTP_Status::NO_CONTENT);

	else
		response.setContent(ErrorPages::getContent(_serverConfig, HTTP_Status::INTERNAL_SERVER_ERROR));
}

const LocationConfig &HTTP_ResponseBuilder::locationGetBestMatch(
	const ServerConfig &sc, const HTTP_Request &req)
{
	std::vector<LocationConfig>::const_iterator selectedLocation_it = sc.locations.end();

	// 1. Strip the query string so we only match against the actual path
	std::string reqURL = req.getURLWithoutParams();

	std::vector<LocationConfig>::const_iterator loc_it = sc.locations.begin();
	for (; loc_it != sc.locations.end(); ++loc_it)
	{
		std::string locPath = loc_it->path;

		if (reqURL + '/' == locPath)
			throw HTTP_ResponseBuilder::Exception(HTTP_Status::FOUND, reqURL + "/");

		// 2. Check if the URL starts with the location path
		if (reqURL.find(locPath) == 0)
		{
			bool isValidMatch = false;

			// 3. Prevent partial word matches (e.g., location "/app" matching URL "/apple")
			if (locPath[locPath.length() - 1] == '/')
			{
				isValidMatch = true; // Location ends in '/', so it's a directory match
			}
			else if (reqURL.length() == locPath.length())
			{
				isValidMatch = true; // Exact match
			}
			else if (reqURL[locPath.length()] == '/')
			{
				isValidMatch = true; // Next character is '/', so it's a clean directory boundary
			}

			DebugLogger(std::cout)("[DEBUG] loc.path = ")(loc_it->path)(" vs reqURL = ")(reqURL)(" --> isValidMatch = ")(isValidMatch)('\n');

			// 4. If it's a valid match, check if it's the longest one we've seen
			if (isValidMatch)
			{
				if (selectedLocation_it == sc.locations.end() ||
					locPath.length() > selectedLocation_it->path.length())
				{
					selectedLocation_it = loc_it;
				}
			}
		}
	}

	if (selectedLocation_it == sc.locations.end())
		// throw std::runtime_error("No suitable server/location found for " + reqURL);
		throw HTTP_ResponseBuilder::Exception(HTTP_Status::NOT_FOUND, reqURL);

	DebugLogger(std::cout)("[DEBUG] Best location match is: ")(selectedLocation_it->path)('\n');
	return (*selectedLocation_it);
}

const LocationConfig &HTTP_ResponseBuilder::locationGetBestMatch(const HTTP_Request &request)
{
	return (HTTP_ResponseBuilder::locationGetBestMatch(_serverConfig, request));
}

//
// This function will attempt to use _location.root + request.getURL...
// If that fails, will attempt _location.alias + request.getURL... and resolve accordingly
// If that fails, will attempt _serverConfig.root + request.getURL...
// If all fail, throw exception!
//
// According to the PDF:
// if location URL /kapouet is mapped to root dir /tmp/www
// then the request URL /kapouet/pouic/toto/pouet
// will have to be searched in /tmp/www/pouic/toto/pouet
//
// the above in nginx is an alias but in webserv has to be
// the default way.
std::string HTTP_ResponseBuilder::translateUriToPath(const HTTP_Request &request)
{
	std::string basePath;
	bool translatingAsAlias = false;

	if (!_location.root.empty())
		basePath = _location.root;
	else if (!_location.alias.empty())
	{
		basePath = _location.alias;
		translatingAsAlias = true;
	}
	else if (!_serverConfig.root.empty())
		basePath = _serverConfig.root;
	else
		throw(HTTP_ResponseBuilder::Exception(
			HTTP_Status::INTERNAL_SERVER_ERROR,
			"location.root, location.alias and serverConfig.root are all empty!"));

	std::string requestURL = request.getURLWithoutParams();

	if (translatingAsAlias)
	{
		if (!replace(requestURL, _location.path, ""))
		{
			std::string errorMsg = "Error: HTTP_ResponseBuilder::translateUriToPath invalid request url \"" + request.getURL() + "\"\n";
			DebugLogger(std::cout)(errorMsg);
			throw(HTTP_ResponseBuilder::Exception(HTTP_Status::BAD_REQUEST, errorMsg));
		}
	}

	return (Utils::joinPath(basePath, requestURL));
}

void HTTP_ResponseBuilder::reset()
{
	_location.allowed_methods.clear();
	_location.autoindex = false;
	_location.cgi_extensions.clear();
	_location.client_max_body_size = 0;
	_location.index = "";
	_location.path = "";
	_location.redirect_code = 0;
	_location.redirect_url = "";
	_location.root = "";
	_location.upload_path = "";

	_pathOnServer = "";
	_pathType = PATH_NONE;
}

// Will return -1 if there was no suitable location found for
// the given request and will set the response accordingly into res.
//
// Will return 0 if neither the location best matching the request
// URL has no client_max_body_size nor the server block of the
// location has client_max_body_size defined.
//
// Will return the client_max_body_size from the best matching
// location or from the server block if the location block does
// not have one!
ssize_t HTTP_ResponseBuilder::getClientMaxBodySize(
	const ServerConfig &sc, const HTTP_Request &req, HTTP_Response &res)
{
	LocationConfig location;

	try
	{
		location = HTTP_ResponseBuilder::locationGetBestMatch(sc, req);
	}
	catch (HTTP_ResponseBuilder::Exception &e)
	{
		if (e.getStatus() == HTTP_Status::FOUND)
		{
			DebugLogger(std::cout)("[DEBUG] HTTP_ResponseBuilder::build - location, canonicalization redirect!")('\n');
			HTTP_ResponseBuilder::setResponseRedirect(res, HTTP_Status::FOUND.code, e.what());
			return (-1);
		}

		// the only other possible exception at the moment is HTTP_Status::NOT_FOUND
		DebugLogger(std::cout)("[DEBUG] HTTP_ResponseBuilder::build - locationGetBestMatch:")('\n')("        ")(e.getStatus().text)(": ")(e.what())('\n');

		HTTP_ResponseBuilder::setResponse(res, HTTP_Status::NOT_FOUND, sc);
		return (-1);
	}

	size_t client_max_body_size = 0;
	if (location.client_max_body_size > 0)
		client_max_body_size = location.client_max_body_size;
	else if (sc.client_max_body_size > 0)
		client_max_body_size = sc.client_max_body_size;

	return (static_cast<ssize_t>(client_max_body_size));
}

size_t HTTP_ResponseBuilder::resolveBodyLimit(
	const ServerConfig &sc, const HTTP_Request &req)
{
	try
	{
		const LocationConfig &location = HTTP_ResponseBuilder::locationGetBestMatch(sc, req);
		if (location.client_max_body_size > 0)
			return (location.client_max_body_size);
		return (sc.client_max_body_size);
	}
	catch (HTTP_ResponseBuilder::Exception &)
	{
		// No matching location (or canonicalization redirect): fall back
		// to the server-level limit. The 404/302 will be produced later
		// by build() in the normal response flow.
		return (sc.client_max_body_size);
	}
}
