/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/05/26 19:22:50 by mosokina         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
#define CGI_HPP

// #include "HTTP/HTTP.hpp"
#include <string>
#include <map>
#include <vector>


#include <unistd.h> // Required for getcwd
#include "Utils.hpp" // AI: suggesting as this already has Utils::getcwd function

class HTTP_Response;
class HTTP_Request;


// Triple returned by executeNonBlocking(): pid, stdin (parent writes
// the body), stdout (parent reads CGI output). All -1 on failure.
struct CGIHandles {
    pid_t   pid;
    int     stdinFd;
    int     stdoutFd;

    CGIHandles() : pid(-1), stdinFd(-1), stdoutFd(-1) {}
    static CGIHandles failure() { return CGIHandles(); }
    bool ok() const { return pid > 0; }
};


class CGILauncher {
    public:
        CGILauncher(const std::string& cgi_path, const std::string& script_path, const HTTP_Request& request);
        ~CGILauncher();

        // NON-BLOCKING: Start CGI process. Caller is responsible for
        // writing the request body to stdinFd (non-blocking) and reading
        // CGI output from stdoutFd. Both FDs are non-blocking on return.
        CGIHandles executeNonBlocking();

        //check if a file extension can be handled by CGI.
        static bool forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);
        static std::string getCGIPath(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);

    private:
        std::string _cgiPath;
        std::string _scriptPath;
        const HTTP_Request& _request;
        std::map<std::string, std::string> _env;

        void setupEnvironment(const std::string& script_name);
        char** createEnvArray();
        void freeEnvArray(char** env);
        std::string extractPathInfo(const std::string& uri, const std::string& script_name);

};

#endif
