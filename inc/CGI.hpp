/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/05/24 04:47:59 by aistok           ###   ########.fr       */
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


class CGI {
    public:
        CGI(const std::string& cgi_path, const std::string& script_path, const HTTP_Request& request);
        ~CGI();

        // NON-BLOCKING: Start CGI process and return pipe fd for reading output
        // Returns: pair<pid, stdoutFd> or <-1, -1> on error
        std::pair<pid_t, int> executeNonBlocking();

        //Parse CGI output into HTTP response
        static HTTP_Response parseCGIOutput(const std::string& output);

        //check if a file extension can be handled by CGI.
        static bool forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);
        static std::string getCGIPath(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);

    private:
        std::string _cgi_path;
        std::string _script_path;
        const HTTP_Request& _request;
        std::map<std::string, std::string> _env;

        void setupEnvironment(const std::string& script_name);
        char** createEnvArray();
        void freeEnvArray(char** env);
        std::string extractPathInfo(const std::string& uri, const std::string& script_name);

};

#endif
