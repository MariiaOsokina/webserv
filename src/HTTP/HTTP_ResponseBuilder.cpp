/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_ResponseBuilder.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/20 10:48:39 by aistok            #+#    #+#             */
/*   Updated: 2026/05/24 14:30:27 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HTTP/HTTP_ResponseBuilder.hpp"

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

HTTP_ResponseBuilder::HTTP_ResponseBuilder(const HTTP_ResponseBuilder &other)
{
	if (this != &other)
	{
		_serverConfig = other._serverConfig;
		_location = other._location;
		_pathOnServer = other._pathOnServer;
		_pathType = other._pathType;
	}
}

HTTP_ResponseBuilder &HTTP_ResponseBuilder::operator=(const HTTP_ResponseBuilder &other)
{
	if (this != &other)
	{
		_serverConfig = other._serverConfig;
		_location = other._location;
		_pathOnServer = other._pathOnServer;
		_pathType = other._pathType;
	}
	return (*this);
}

void HTTP_ResponseBuilder::build(HTTP_Response &response, HTTP_Request &request)
{
	// DEBUG - TO-DO: remove these!
	std::cout << "++ GOT REQUEST ++" << std::endl;
	std::cout << request.getDisplayFriendlyRequest();
	std::cout << "++ REQUEST FIN ++" << std::endl;

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
			std::cout << "[DEBUG] HTTP_ResponseBuilder::build - location, canonicalization redirect!" << std::endl;
			HTTP_ResponseBuilder::setResponseRedirect(response, HTTP_Status::FOUND.code, e.what());
			return;
		}

		// the only other possible exception at the moment is HTTP_Status::NOT_FOUND
		std::cout << "[DEBUG] HTTP_ResponseBuilder::build - locationGetBestMatch:"
				  << std::endl
				  << "        " << e.getStatus().text << ": " << e.what()
				  << std::endl;

		setResponse(response, HTTP_Status::NOT_FOUND);
		return;
	}

	if (_location.redirect_code > 0) // TO-DO: needs improving, redirect_code is always ZERO from config parser
	{
		HTTP_ResponseBuilder::setResponseRedirect(response, _location.redirect_code, _location.redirect_url);
		return;
	}

	if (!locationHasMethod(_location, method))
	{
		setResponse(response, HTTP_Status::METHOD_NOT_ALLOWED);
		return;
	}

	try
	{
		_pathOnServer = translateUriToPath(request);
		std::cout << "[DEBUG] pathOnServer: " << _pathOnServer << "\n";
	}
	catch (HTTP_ResponseBuilder::Exception &e)
	{
		std::cout << "[DEBUG] HTTP_ResponseBuilder::translateUriToPath : " << e.what() << std::endl;
		setResponse(response, e.getStatus());
		return;
	}

	_pathType = getPathType(_pathOnServer);

	// 3. MO: NEW CGI Logic Integration
	if (_pathType == PATH_FILE && (method == "GET" || method == "POST")) // AI: Any method should be allowed here, not just GET and POST
	{
		// --- MOVE DEBUG HERE (Outside the if block) ---
		std::string ext = Utils::getExtension(_pathOnServer);
		std::cout << "[DEBUG] Checking CGI for Path: " << _pathOnServer << std::endl;
		std::cout << "[DEBUG] Ext extracted: [" << ext << "]" << std::endl;
		std::cout << "[DEBUG] Is ext in map? " << (CGI::forCGIResponse(_pathOnServer, _location.cgi_extensions) ? "YES" : "NO") << std::endl;

		if (CGI::forCGIResponse(_pathOnServer, _location.cgi_extensions))
		{

			// We get the executable path (e.g., /usr/bin/python3)
			std::string cgi_path = CGI::getCGIPath(_pathOnServer, _location.cgi_extensions);

			// Flag the response as CGI
			response.setCGIGenerated(true);

			// NOTE: add these two setters to  HTTP_Response class!
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

	std::cout << "[DEBUG] _serverConfig.client_max_body_size --> " << _serverConfig.client_max_body_size << std::endl;
	std::cout << "[DEBUG] _location.client_max_body_size --> " << _location.client_max_body_size << std::endl;

	if (response.isCGIGenerated())
	{
		std::cout << "[DEBUG] CGI Will be used!" << std::endl;
		return;
	}
	else if (method == HTTP_Method::GET || method == HTTP_Method::HEAD)
	{
		std::cout << "[DEBUG] Handling GET request! (method = " << method << ")" << std::endl;
		build_response_for_GET_or_HEAD(response, request);
	}
	else if (method == HTTP_Method::POST)
	{
		std::cout << "[DEBUG] Handling POST request!" << std::endl;
		build_response_for_POST(response, request);
	}
	else if (method == HTTP_Method::DELETE)
	{
		std::cout << "[DEBUG] Handling DELETE request!" << std::endl;
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

bool HTTP_ResponseBuilder::locationHasMethod(const LocationConfig &location, std::string method)
{
	std::vector<std::string>::const_iterator method_it = location.allowed_methods.begin();
	for (; method_it != location.allowed_methods.end(); ++method_it)
	{
		if (*method_it == method)
			return (true);
	}
	return (false);
}

void HTTP_ResponseBuilder::build_response_for_GET_or_HEAD(HTTP_Response &response, HTTP_Request &request)
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
			std::cout << "[DEBUG] +++ URL normalization" << std::endl;
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
					std::cout << "[DEBUG] Unable to open file " << indexOnServer << std::endl;
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
	// DEBUGGING
	// std::cout << "++ GOT REQUEST ++" << std::endl;
	// std::cout << request; //.getDisplayFriendlyRequest();
	// std::cout << "++ REQUEST FIN ++" << std::endl;
	// std::string filename = Utils::getNextAvailableFilename("request.dat");
	// Utils::writeStringToFile(filename, request.serialize());
	// std::cout << "Request saved to " << filename << std::endl;

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
		std::cout << "[DEBUG] ERROR: " << errorMsg << std::endl;
		setResponse(response, HTTP_Status::INTERNAL_SERVER_ERROR);
		return;
	}

	if (!request.isMultipartRequest())
	{
		std::cout << "[DEBUG] Request is NOT multipart!" << std::endl;

		filenameOnServer = Utils::joinPath(_location.upload_path, DEFAULT_UPLOAD_FILENAME);
		dataToWrite = Utils::urlDecode(request.getBody());
	}
	else
	{
		std::cout << "[DEBUG] Request IS multipart!" << std::endl;
		if (request.populateMultipartVars() == FAILURE)
		{
			std::cout << "[DEBUG] ERROR reading multipart request!" << std::endl;
			setResponse(response, HTTP_Status::BAD_REQUEST);
			return;
		}

		std::cout << "[DEBUG] Boundary: " << request._multipartBoundary << std::endl;
		std::cout << "[DEBUG] Filename: " << request._multipartFilename << std::endl;
		std::cout << "[DEBUG] Data start>>>" << request._multipartData << "<<<Data fin" << std::endl;

		filenameOnServer = Utils::joinPath(_location.upload_path, request._multipartFilename);

		if (filenameOnServer == _location.upload_path)
			filenameOnServer = Utils::joinPath(_location.upload_path, DEFAULT_UPLOAD_FILENAME);

		dataToWrite = request._multipartData;
	}

	filenameOnServer = Utils::getNextAvailableFilename(filenameOnServer);

	std::ofstream fileOnServer(filenameOnServer.c_str(), std::ios::binary);
	if (!fileOnServer.is_open())
	{
		std::cout << "[DEBUG] Upload error: Could not open file " << filenameOnServer << std::endl;
		setResponse(response, HTTP_Status::INTERNAL_SERVER_ERROR);
		return;
	}

	fileOnServer.write(dataToWrite.c_str(), dataToWrite.size());
	fileOnServer.close();

	std::cout << "[INFO] Uploaded file saved to " << filenameOnServer
			  << " (" << dataToWrite.size() << " bytes)"
			  << std::endl;

	response.setStatus(HTTP_Status::CREATED);
	response.setContent("File upload successfull!");
	return;
}

void HTTP_ResponseBuilder::build_response_for_DELETE(
	HTTP_Response &response,
	HTTP_Request &request)
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

			// DEBUG // TO-DO: remove!!!
			std::cout << "loc.path = " << loc_it->path << " vs reqURL = " << reqURL << " --> isValidMatch = " << isValidMatch << std::endl;

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

	std::cout << "[DEBUG] Best location match is: " << selectedLocation_it->path << std::endl;
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
			std::cout << errorMsg;
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
			std::cout << "[DEBUG] HTTP_ResponseBuilder::build - location, canonicalization redirect!" << std::endl;
			HTTP_ResponseBuilder::setResponseRedirect(res, HTTP_Status::FOUND.code, e.what());
			return (-1);
		}

		// the only other possible exception at the moment is HTTP_Status::NOT_FOUND
		std::cout << "[DEBUG] HTTP_ResponseBuilder::build - locationGetBestMatch:"
				  << std::endl
				  << "        " << e.getStatus().text << ": " << e.what()
				  << std::endl;

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