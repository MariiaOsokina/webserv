#ifndef CGI_HPP
#define CGI_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <string>
#include <map>
#include <vector>

class CGI {
    public:
        CGI(const std::string& cgi_path, const std::string& script_path, const HttpRequest& request);
        ~CGI();

        //Prev-- Blocking: Execute CGI and return response
        HttpResponse execute();

        // NON-BLOCKING: Start CGI process and return pipe fd for reading output
        // Returns: pair<pid, stdout_fd> or <-1, -1> on error
        std::pair<pid_t, int> executeNonBlocking();

        //Parse CGI output into HTTP response
        static HttpResponse parseCGIOutput(const std::string& output);

        //check if a file extension can be handled by CGI.
        static bool forCGIResponse(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);
        static std::string getCGIPath(const std::string& filepath, const std::map<std::string, std::string>& cgi_map);

    private:
        std::string _cgi_path;
        std::string _script_path;
        const HttpRequest& _request;
        std::map<std::string, std::string> _env;

        void setupEnvironment(const std::string& script_name);
        char** createEnvArray();
        void freeEnvAray(char** env);
        std::string extractPathInfo(const std::string& uri, const std::string& script_name);

};

#endif