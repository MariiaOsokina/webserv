#include "CGI.hpp"
#include "Utils.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>

CGI::CGI(const std::string& cgi_path, const std::string& script_path, const HttpRequest& request)
    : _cgi_path(cgi_path), _script_path(script_path), _request(request) {
    setupEnvironment(script_path);
}

CGI::~CGI() {}

void CGI::setupEnvironment(const std::string& script_name) {
    //CGI/1.1 environment variables
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["SERVER_PROTOCOL"] = _request.getHttpVersion();
    _env["SERVER_SOFTWARE"] = "webserve/1.0";
    _env["REQUEST_METHOD"] = _request.getMethod();

    //Parse URI and query string
    std::string uri = _request.getUri();
    size_t query_pos = uri.find('?');
    std::string script_path = uri;
    std::string query_string;

    if (query_pos != std::string::npos) {
        script_path = uri.substr(0, query_pos);
        query_string = uri.substr(query_pos + 1);
    }

    std::string absolute = "/home/lexymma/Documents/42/MWebserve"; //Newly added
    _env["REDIRECT_STATUS"] = "200"; //Newly added
    _env["SCRIPT_NAME"] = script_path;
    _env["SCRIPT_FILENAME"] = absolute + script_name.substr(1);
    _env["QUERY_STRING"] = query_string;
    _env["PATH_INFO"] = extractPathInfo(uri, script_path);

    std::cout << "SCRIPT_FILENAME = " << _env["SCRIPT_FILENAME"] << std::endl;

    //Request Headers
    std::string content_length = _request.getHeader("content-length");  
    std::string content_type = _request.getHeader("content-type");

    if (!content_length.empty()) {
        _env["CONTENT_LENGTH"] = content_length;
    }
    if (!content_type.empty()) {
        _env["CONTENT_TYPE"] = content_type;
    }

    //Http headers (convert to HTTP_* format)
    const std::map<std::string, std::string>& headers = _request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); 
        it != headers.end(); ++it) {
        if (it->first != "content-length" && it->first != "content-type") {
            std::string name = "HTTP_" + Utils::toUpperCase(it->first);
            //We replace with _
            for (size_t i = 0; i < name.length(); ++i) {
                if (name[i] == '-') {
                    name[i] = '_';
                }
            }
            _env[name] = it->second;
        }
    }
    //Server info
    _env["SERVER_NAME"] = "localhost";
    _env["SERVER_PORT"] = "8080";

    //Remote info
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

    for (std::map<std::string, std::string>::const_iterator it = _env.begin();
         it != _env.end(); ++it, ++i) {
        std::string env_str = it->first + "=" + it->second;
        env[i] = new char[env_str.length() + 1];
        std::strcpy(env[i], env_str.c_str());
    }

    env[i] = NULL;
    return (env);
}

void CGI::freeEnvAray(char** env) {
    for (size_t i = 0; env[i] != NULL; ++i) {
        delete[] env[i];
    }
    delete[] env;
}

HttpResponse CGI::execute() {
    int pipe_in[2]; //For sending request body to CGI
    int pipe_out[2]; //For receiving CGI output

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        return (HttpResponse::errorResponse(500));
    }

    pid_t pid = fork();
    //In case of fork failure we close all pipes created.
    if (pid < 0) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return (HttpResponse::errorResponse(500));
    }
    
    if (pid == 0) {
        //Child process
        //Redirect stdin to pipe_in
        dup2(pipe_in[0], STDIN_FILENO);
        close(pipe_in[0]);
        close(pipe_in[1]);

        //Redirect stdout to pipe_out
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[0]);
        close(pipe_out[1]);

        //Change to script directory
        std::string dir = Utils::getDirectory(_script_path);
        if (!dir.empty()) {
            chdir(dir.c_str());
        }

        //Prepare arguments
        char* args[3];
        args[0] = const_cast<char*>(_cgi_path.c_str());
        args[1] = const_cast<char*>(_script_path.c_str());
        args[2] = NULL;

        //Prepare env
        char** env = createEnvArray();

        //Execute CGI
        execve(_cgi_path.c_str(), args, env);
        
        exit(1); //If we get here execve failed.
    }
    //Parent process
    close(pipe_in[0]);
    close(pipe_out[1]);

    //Write request body to CGI stdin
    const std::string& body = _request.getBody();
    if (!body.empty()) {
        write(pipe_in[1], body.c_str(), body.length());
    }
    close(pipe_in[1]);

    //Read CGI output
    std::string output;
    char buffer[8192];
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_out[0], buffer, sizeof(buffer))) > 0)  {
        output.append(buffer, bytes_read);
    }
    close(pipe_out[0]);

    int status;
    waitpid(pid, &status, 0); //wait for child process
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return (HttpResponse::errorResponse(500));
    }

    //Parse CGI output (headers and body seperated by empty line)
    HttpResponse response(200);

    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = output.find("\n\n");
        if (header_end != std::string::npos) {
            header_end += 2;
        }
    } else {
        header_end +=4;
    }

    if (header_end != std::string::npos) {
        std::string headers_part = output.substr(0, header_end);
        std::string body_part = output.substr(header_end);

        //Parse headers
        std::vector<std::string> header_lines = Utils::split(headers_part, '\n');
        for (size_t i = 0; i < header_lines.size(); ++i) {
            std::string line = Utils::trim(header_lines[i]);
            if (line.empty())
                continue;

            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = Utils::trim(line.substr(0, colon));
                std::string value = Utils::trim(line.substr(colon + 1));

                if (Utils::toLowerCase(name) == "status") {
                    int code = Utils::toInt(value);
                    response.setStatusCode(code);
                } else {
                    response.setHeader(name, value);
                }
            }
        }
        response.setBody(body_part);
    } else {
        response.setBody(output);
        response.setHeader("Content-Type", "text/html");
    }
    return (response);
}

std::pair<pid_t, int>CGI::executeNonBlocking() {
    int pipe_in[2]; //For sending request body into child process
    int pipe_out[2]; //For receiving CGI output from child process

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        return (std::make_pair(-1, -1));
    }

    pid_t pid = fork();

    if (pid < 0) {
        close(pipe_in[0]);
        close(pipe_in[0]);
        close(pipe_out[1]);
        return (std::make_pair(-1, -1));
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

        //Prep args
        char* args[2];
        args[0] = const_cast<char*>(_cgi_path.c_str());
        args[1] = NULL;

        //prep env
        char** env = createEnvArray();
        
        //Execute CGI
        execve(_cgi_path.c_str(), args, env);

        //If we get here, execve failed
        exit(1);
    }

    //Parent process
    close(pipe_in[0]); //Close read end of input
    close(pipe_out[1]); //close write end of output

    //Write request body to CGI stdin (non-blocking write)
    const std::string& body = _request.getBody();
    if (!body.empty()) {
        //This write could block for large bodies, for a robust implementation
        //pipe_in[1] should be set to non-blocking, then handle partual writes.
        //However this acceptable for a typical CGI.
        write(pipe_in[1], body.c_str(), body.length());
    }
    close(pipe_in[1]); //After closing stdin, CGI gets EOF

    //Set pipe_out to non-blocking
    int flags = fcntl(pipe_out[0], F_GETFL, 0);
    fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);

    //return pid and stdout fd for parent to monitor with poll
    return std::make_pair(pid, pipe_out[0]);
}

HttpResponse CGI::parseCGIOutput(const std::string& output) {
    // Check if CGI is using NPH (Non-Parsed Headers) mode
    // NPH CGI scripts output the complete HTTP response including status line
    std::cout << "parseCGIOutput: checking NPH, length=" << output.length() << std::endl;
    if (output.length() > 5) {
        std::cout << "First 5 chars: [" << output.substr(0, 5) << "]" << std::endl;
    }
    
    if (output.length() > 5 && output.substr(0, 5) == "HTTP/") {
     
        // std::cout << "*** NPH MODE DETECTED! Sending raw output ***" << std::endl;
        // NPH mode - CGI already sent complete HTTP response
        // We need to send this raw output directly without adding our headers
        HttpResponse response(200);
        
        // Parse the status line to get the actual status code
        size_t first_line_end = output.find("\r\n");
        if (first_line_end == std::string::npos) {
            first_line_end = output.find("\n");
        }
        
        if (first_line_end != std::string::npos) {
            std::string status_line = output.substr(0, first_line_end);
            // Parse "HTTP/1.1 200 OK" to extract 200
            std::vector<std::string> parts = Utils::split(status_line, ' ');
            if (parts.size() >= 2) {
                int status_code = Utils::toInt(parts[1]);
                response.setStatusCode(status_code);
            }
        }
        // Store the complete CGI output as raw response
        // We'll need to send this directly without buildRawResponse()
        response.setRawOutput(output);
        return response;
    }
    
    HttpResponse response(200);
    
    size_t header_end = output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = output.find("\n\n");
        if (header_end != std::string::npos) {
            header_end += 2;
        }
    } else {
        header_end += 4;
    }
    
    if (header_end != std::string::npos) {
        std::string headers_part = output.substr(0, header_end);
        std::string body_part = output.substr(header_end);
        
        // Parse headers
        std::vector<std::string> header_lines = Utils::split(headers_part, '\n');
        for (size_t i = 0; i < header_lines.size(); ++i) {
            std::string line = Utils::trim(header_lines[i]);
            if (line.empty()) continue;
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = Utils::trim(line.substr(0, colon));
                std::string value = Utils::trim(line.substr(colon + 1));
                
                if (Utils::toLowerCase(name) == "status") {
                    int code = Utils::toInt(value);
                    response.setStatusCode(code);
                } else {
                    response.setHeader(name, value);
                }
            }
        }
        response.setBody(body_part);
    } else {
        // No headers, just body
        response.setBody(output);
        response.setHeader("Content-Type", "text/html");
    }
    
    return response;
}

bool CGI::forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map) {
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
