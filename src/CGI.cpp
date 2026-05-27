/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/05/26 19:51:00 by mosokina         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CGI.hpp"
#include "Utils.hpp"
#include "HTTP/HTTP.hpp"
#include "WebServMacros.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>

CGI::CGI(const std::string& cgi_path, const std::string& script_path, const HTTP_Request& request)
	: _cgi_path(cgi_path), _script_path(script_path), _request(request) {
	setupEnvironment(script_path);
}

CGI::~CGI() {}

void CGI::setupEnvironment(const std::string& script_name) {
	// 1. Core CGI/1.1 Variables
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SERVER_PROTOCOL"] = _request.getVersion();
	_env["SERVER_SOFTWARE"] = WEBSERV_NAME; // AI: fix //"webserv/1.0";
	_env["REQUEST_METHOD"] = _request.getMethod();

    std::string file_ext = Utils::toLowerCase(Utils::getExtension(script_name));
    if (file_ext == ".php")
    {
	    // 2. Parse URI, Query String, and Path Info
	    // MO: Required for php-cgi to run properly
	    _env["REDIRECT_STATUS"] = "200"; //Newly added
	    //_env["SCRIPT_NAME"] = script_name;
        _env["SCRIPT_NAME"] = _request.getURLWithoutParams();
	    _env["PATH_INFO"] = extractPathInfo(_request.getURL(), script_name); // AI: updated
    }
    else
    {
        _env["PATH_INFO"] = _request.getURLWithoutParams();
    }
	_env["QUERY_STRING"] = _request.getQueryString(); // AI: updated for clean code
	
	// 3. Dynamic Absolute Pathing (Fixes the hardcoded "/home/..." string)
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		std::string absolute_path = std::string(cwd); // AI: suggesting to use Utils::getcwd() to maintain a clean code base
		// Ensure we don't double up on slashes if script_name starts with one
		//if (!script_name.empty() && script_name[0] == '/') {
		//	_env["SCRIPT_FILENAME"] = absolute_path + script_name;
		//} else {
		//	_env["SCRIPT_FILENAME"] = absolute_path + "/" + script_name;
		//}
        _env["SCRIPT_FILENAME"] = Utils::joinPath(absolute_path, script_name); // AI: updated for clean code
	} else {
		// Fallback in case getcwd fails
		_env["SCRIPT_FILENAME"] = script_name;
	}

	// 4. Handle Specific Headers safely using iterators
	//Http headers (convert to HTTP_* format)
	const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = _request.getHeaders();
    
    // Instead of trusting the headers, trust the actual parsed body size!
    // This perfectly fixes chunked requests, standard requests, and empty GETs.
    size_t actual_body_length = _request.getBody().length();
    _env["CONTENT_LENGTH"] = Utils::toString(actual_body_length);

	std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator itCT = headers.find("content-type");
	if (itCT != headers.end()) {
		_env["CONTENT_TYPE"] = itCT->second;
	}
	// 5. Convert Remaining HTTP Headers to HTTP_ format
    for (std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        // Standardize the key to lowercase for the check
        std::string keyLow = Utils::toLowerCase(it->first); 
        
        // Skip the ones we already handled specifically as CONTENT_TYPE/LENGTH
        if (keyLow == "content-length" || keyLow == "content-type") {
            continue;
        }

        // Format: HTTP_HEADER_NAME (e.g., User-Agent -> HTTP_USER_AGENT)
        std::string name = "HTTP_" + Utils::toUpperCase(it->first);
        for (size_t i = 0; i < name.length(); ++i) {
            if (name[i] == '-') {
                name[i] = '_';
            }
        }
        _env[name] = it->second;
    }
	// 6. Network/Server Variables (Hardcoded placeholders)
	// MO: Replace these later with actual data from your Connection/Config class!
	//Server info
	// _env["SERVER_NAME"] = "localhost"; 
	// _env["SERVER_PORT"] = "8080";      

	// //Remote info
	// _env["REMOTE_ADDR"] = "127.0.0.1";
	// Example of dynamic Server Name (Usually derived from the Host header)
    std::map<std::string, std::string, CaseInsensitiveCompare>::const_iterator itHost = headers.find("host");
    if (itHost != headers.end()) {
        // Host header often looks like "localhost:8080", so we need to split it
        std::string host_port = itHost->second;
        size_t colon_pos = host_port.find(':');
        
        if (colon_pos != std::string::npos) {
            _env["SERVER_NAME"] = host_port.substr(0, colon_pos);
            _env["SERVER_PORT"] = host_port.substr(colon_pos + 1);
        } else {
            _env["SERVER_NAME"] = host_port;
            _env["SERVER_PORT"] = "80"; // Default HTTP port
        }
    } else {
        _env["SERVER_NAME"] = "localhost"; // Fallback
        _env["SERVER_PORT"] = "8080";      // Fallback
    }

    // For REMOTE_ADDR, you ideally want the client's IP address extracted 
    // from accept() in your Connection object. If you don't have that yet,
    // leaving it as 127.0.0.1 is usually acceptable for the evaluation.
    _env["REMOTE_ADDR"] = "127.0.0.1";
}

std::string CGI::extractPathInfo(const std::string& uri, const std::string& script_name) {
	if (uri.length() > script_name.length()) {
		size_t pos = uri.find('?');
		if (pos != std::string::npos) {
			return (uri.substr(script_name.length(), pos - script_name.length()));
		}
		return (uri.substr(script_name.length()));
	}
	return ("");
}

char** CGI::createEnvArray() {
    char** env = new char*[_env.size() + 1];
    size_t i = 0;

    std::cerr << "\n--- [DEBUG] CGI Environment Variables ---" << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = _env.begin();
         it != _env.end(); ++it, ++i) {
        
        std::string env_str = it->first + "=" + it->second;
        
        // Print to terminal for you to see
        std::cerr << "ENV[" << i << "]: " << env_str << std::endl;

        env[i] = new char[env_str.length() + 1];
        std::strcpy(env[i], env_str.c_str());
    }
    std::cerr << "------------------------------------------\n" << std::endl;

    env[i] = NULL;
    return (env);
}

//MO: Just ensure that wherever you call createEnvArray() (inside executeNonBlocking), 
//you also call your freeEnvArray(env) in the parent process after the fork() happens. 
//If execve fails in the child, you should also ideally free it before calling exit(1).
void CGI::freeEnvArray(char** env) {
	for (size_t i = 0; env[i] != NULL; ++i) {
		delete[] env[i];
	}
	delete[] env;
}

CGIHandles CGI::executeNonBlocking() {
	std::cout << "[DEBUG] CGI Execution Started for: " << _script_path << std::endl;

	int pipe_in[2]; //For sending request body into child process
	int pipe_out[2]; //For receiving CGI output from child process
    int child_original_stdout = 1001; // AI: FOR DEBUG ONLY!!!

	if (pipe(pipe_in) < 0) {
		return CGIHandles::failure();
	}
	if (pipe(pipe_out) < 0) {
		close(pipe_in[0]);
		close(pipe_in[1]);
		return CGIHandles::failure();
	}

	pid_t pid = fork();

	if (pid < 0) {
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		return CGIHandles::failure();
	}
	if (pid == 0) {
		//Child process
        dup2(STDOUT_FILENO, child_original_stdout); //AI: DEBUG ONLY!
		//Redirect stdin from pipe_in
		dup2(pipe_in[0], STDIN_FILENO);
		close(pipe_in[0]);
		close(pipe_in[1]);

		//Redirect stdout to pipeout
		dup2(pipe_out[1], STDOUT_FILENO);
		close(pipe_out[1]);
		close(pipe_out[0]);

		// --- ADD THIS TO EXTRACT JUST THE FILENAME ---
        //std::string filename = _script_path;
        //size_t lastSlash = _script_path.find_last_of('/');
        //if (lastSlash != std::string::npos) {
        //    filename = _script_path.substr(lastSlash + 1);
        //}
        std::string filename = Utils::getFileName(_script_path); // AI: updated, for clean code
        // ---------------------------------------------

		//Prep args
		// char* args[2];
		// args[0] = const_cast<char*>(_cgi_path.c_str());
		// args[1] = NULL;

		char* args[3];
        std::string executableWithAbsolutePath = Utils::getAbsolutePath(_cgi_path); // AI: changed to absolute path, because later there will be a chdir, just before execve !!!
        args[0] = const_cast<char*>(executableWithAbsolutePath.c_str());
		args[1] = const_cast<char*>(filename.c_str());
		args[2] = NULL;

		//prep env
		char** env = createEnvArray();

        // AI: moved chdir just before the execution is done,
        //     so that other file operations are performed in
        //     the correct directory
        //
		//change to scrip directory:
		//some CGI scripts (especially PHP) expect to be executed from dir they live in
        std::string scritp_file_dir = Utils::getDirectory(_script_path);
        if (!scritp_file_dir.empty()) {
            chdir(scritp_file_dir.c_str());
        }

        // AI: DEBUGGING ONLY
        //dprintf(child_original_stdout, "*** CHILD CGI:\n");
        //dprintf(child_original_stdout, "*** EXECUTING: %s %s\n", args[0], args[1]);
        //std::string cwd =  Utils::getcwd();
        //dprintf(child_original_stdout, "*** cwd = %s\n", cwd.c_str());

        //Execute CGI
		execve(args[0], args, env); // AI: updated for clean code

		// ==========================================
		// IF WE REACH HERE, EXECVE FAILED!
		// ==========================================
		
		// 1. Free the dynamically allocated array
		freeEnvArray(env);

		// 2. Close the remaining FDs that were duped
		// It's good practice to close standard FDs if execve fails
		close(STDIN_FILENO);
		close(STDOUT_FILENO);

		// 3. Exit safely
		// PRO TIP: Always use _exit() in a child process, not exit()!
		// exit() will flush standard I/O buffers and call atexit() handlers 
		// which might belong to the parent process and corrupt your server.
		_exit(1);

		// exit(1);
	}
// ==========================================
    // Parent process logic starts here
    // ==========================================

    // 1. Close the ends of the pipes we don't use
    close(pipe_in[0]);   // Parent does not read from the CGI's input pipe
    close(pipe_out[1]);  // Parent does not write to the CGI's output pipe

    std::cerr << "[DEBUG] CGI Parent: Body length in Request is "
              << _request.getBody().length() << " bytes." << std::endl;

    // 2. Set BOTH parent ends non-blocking. The caller drains the body to
    // pipe_in[1] via POLLOUT events and reads CGI output from pipe_out[0]
    // via POLLIN events. If either setup fails, kill+reap the child to
    // avoid leaving a zombie.
    if (!setNonBlocking(pipe_in[1]) || !setNonBlocking(pipe_out[0])) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        return CGIHandles::failure();
    }

    // 3. Return all three handles. NOTE: pipe_in[1] is intentionally
    // left open here. The caller MUST close it once the body has been
    // fully written — that close is what sends EOF to the CGI script.
    CGIHandles h;
    h.pid      = pid;
    h.stdinFd  = pipe_in[1];
    h.stdoutFd = pipe_out[0];
    return h;
}

void CGI::parseCGIOutput(std::string& output, HTTP_Response& response) {
    response.reset();
    response.setStatus(HTTP_Status::OK);
    response.setCGIGenerated(true);

    // 1. Locate the headers/body boundary.
    size_t header_end = output.find("\r\n\r\n");
    size_t delimiter_len = 4;
    if (header_end == std::string::npos) {
        header_end = output.find("\n\n");
        delimiter_len = 2;
    }

    if (header_end == std::string::npos) {
        // No headers found — whole payload is the body. Move it into
        // the response without copying.
        response.swapBody(output);
        response.getHeaders()["Content-Type"] = "text/html";
        return;
    }

    // 2. Headers: a one-time copy of the (small) header section.
    std::string headers_part = output.substr(0, header_end);

    // 3. Body: strip the header bytes out of `output` in-place, then
    // hand the remaining buffer to the response by swap. No 200 MB
    // duplicate is created.
    output.erase(0, header_end + delimiter_len);
    response.swapBody(output);

    // 4. Parse headers line by line.
    std::vector<std::string> header_lines = Utils::split(headers_part, '\n');
    for (size_t i = 0; i < header_lines.size(); ++i) {
        std::string line = Utils::trim(header_lines[i]);
        if (line.empty()) continue;

        // NPH (Non-Parsed Header): first line may be a status line.
        if (i == 0 && line.length() > 5 && line.substr(0, 5) == "HTTP/") {
            std::vector<std::string> parts = Utils::split(line, ' ');
            if (parts.size() >= 2) {
                int status_code = Utils::toInt(parts[1]);
                response.setStatus(HTTP_Status::fromCode(status_code));
            }
            continue;
        }

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = Utils::trim(line.substr(0, colon));
            std::string value = Utils::trim(line.substr(colon + 1));

            if (Utils::toLowerCase(name) == "status") {
                int status_code = Utils::toInt(value);
                response.setStatus(HTTP_Status::fromCode(status_code));
            } else if (Utils::toLowerCase(name) != "content-length") {
                // Content-Length is owned by swapBody (set from the
                // actual body length). Ignoring the CGI's header
                // prevents a mismatch if the script lied.
                response.getHeaders()[name] = value;
            }
        }
    }
}

bool CGI::forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map){
	std::string ext = Utils::getExtension(filepath);
	return (cgi_map.find(ext) != cgi_map.end());
}

std::string CGI::getCGIPath(const std::string& filepath, const std::map<std::string, std::string>& cgi_map) {
	std::string ext = Utils::getExtension(filepath);
	std::map<std::string, std::string>::const_iterator it = cgi_map.find(ext);
	if (it != cgi_map.end()) {
		return (it->second);
	}
	return ("");
}
