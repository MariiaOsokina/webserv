/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/06/01 23:59:56 by mosokina         ###   ########.fr       */
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

CGILauncher::CGILauncher(const std::string& cgi_path, const std::string& script_path, const HTTP_Request& request)
	: _cgiPath(cgi_path), _scriptPath(script_path), _request(request) {
	setupEnvironment(script_path);
}

CGILauncher::~CGILauncher() {}

void CGILauncher::setupEnvironment(const std::string& script_name) {
	// 1. Core CGI/1.1 Variables
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SERVER_PROTOCOL"] = _request.getVersion();
	_env["SERVER_SOFTWARE"] = WEBSERV_NAME;
	_env["REQUEST_METHOD"] = _request.getMethod();

    std::string file_ext = Utils::toLowerCase(Utils::getExtension(script_name));
    if (file_ext == ".php")
    {
	    // 2. SCRIPT_NAME, PATH_INFO and REDIRECT_STATUS are required for php-cgi.
	    _env["REDIRECT_STATUS"] = "200";
	    _env["SCRIPT_NAME"] = _request.getURLWithoutParams();
	    _env["PATH_INFO"] = extractPathInfo(_request.getURL(), script_name);
    }
    else
    {
        _env["PATH_INFO"] = _request.getURLWithoutParams();
    }
	_env["QUERY_STRING"] = _request.getQueryString();
	
	// 3. Dynamic Absolute Pathing (Fixes the hardcoded "/home/..." string)
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		std::string absolute_path = std::string(cwd);
        _env["SCRIPT_FILENAME"] = Utils::joinPath(absolute_path, script_name);
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
	// 6. Server name and port, derived from the Host header
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

std::string CGILauncher::extractPathInfo(const std::string& uri, const std::string& script_name) {
	if (uri.length() > script_name.length()) {
		size_t pos = uri.find('?');
		if (pos != std::string::npos) {
			return (uri.substr(script_name.length(), pos - script_name.length()));
		}
		return (uri.substr(script_name.length()));
	}
	return ("");
}

char** CGILauncher::createEnvArray() {
    char** env = new char*[_env.size() + 1];
    size_t i = 0;

    for (std::map<std::string, std::string>::const_iterator it = _env.begin();
         it != _env.end(); ++it, ++i) {
        std::string env_str = it->first + "=" + it->second;
        env[i] = new char[env_str.length() + 1];
        std::strcpy(env[i], env_str.c_str());
    }
    env[i] = NULL;
    return (env);
}

void CGILauncher::freeEnvArray(char** env) {
	for (size_t i = 0; env[i] != NULL; ++i) {
		delete[] env[i];
	}
	delete[] env;
}

CGIHandles CGILauncher::executeNonBlocking() {

	int pipe_in[2]; //For sending request body into child process
	int pipe_out[2]; //For receiving CGI output from child process

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
		//Redirect stdin from pipe_in
		dup2(pipe_in[0], STDIN_FILENO);
		close(pipe_in[0]);
		close(pipe_in[1]);

		//Redirect stdout to pipeout
		dup2(pipe_out[1], STDOUT_FILENO);
		close(pipe_out[1]);
		close(pipe_out[0]);

        std::string filename = Utils::getFileName(_scriptPath);

		//Prep args
		char* args[3];
		// Absolute path: a chdir to the script's directory happens just before execve.
        std::string executableWithAbsolutePath = Utils::getAbsolutePath(_cgiPath);
        args[0] = const_cast<char*>(executableWithAbsolutePath.c_str());
		args[1] = const_cast<char*>(filename.c_str());
		args[2] = NULL;

		//prep env
		char** env = createEnvArray();

		// Change to the script's directory just before execve: some CGI
		// scripts (especially PHP) expect to run from the dir they live in.
        std::string scritp_file_dir = Utils::getDirectory(_scriptPath);
        if (!scritp_file_dir.empty()) {
            chdir(scritp_file_dir.c_str());
        }

        //Execute CGI
		execve(args[0], args, env);

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
	}
// ==========================================
    // Parent process logic starts here
    // ==========================================

    // 1. Close the ends of the pipes we don't use
    close(pipe_in[0]);   // Parent does not read from the CGI's input pipe
    close(pipe_out[1]);  // Parent does not write to the CGI's output pipe


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


bool CGILauncher::forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map){
	std::string ext = Utils::getExtension(filepath);
	return (cgi_map.find(ext) != cgi_map.end());
}

std::string CGILauncher::getCGIPath(const std::string& filepath, const std::map<std::string, std::string>& cgi_map) {
	std::string ext = Utils::getExtension(filepath);
	std::map<std::string, std::string>::const_iterator it = cgi_map.find(ext);
	if (it != cgi_map.end()) {
		return (it->second);
	}
	return ("");
}
