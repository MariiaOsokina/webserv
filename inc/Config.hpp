/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/05/14 21:39:49 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowed_methods;
    std::string root;
    std::string alias; // AI: added this so that we can run the 42 tester
    bool autoindex;
    std::string index;
    int redirect_code;
    std::string redirect_url;
    std::string upload_path;
    std::map<std::string, std::string> cgi_extensions; //extension -> cgi binary path
    size_t client_max_body_size;

    LocationConfig() : autoindex(false), redirect_code(0), client_max_body_size(0) {}
};

struct ServerConfig
{
    std::vector<int> ports; //ports will remain thesame if you need one just use index [0]
    std::string host; //Now added
    std::vector<std::string> server_names;
    std::map<int, std::string> error_pages; //error_code -> page_path
    std::vector<LocationConfig> locations;
    size_t client_max_body_size;
    std::string root;
    std::string index;

    ServerConfig() : host(""), client_max_body_size(0) {}//1mb default..
};

class ConfigException : public std::exception {
    public:
        ConfigException(const std::string& msg, int line = -1);
        virtual ~ConfigException() throw();
        virtual const char* what() const throw();
        int getLineNumber() const;

    private:
        std::string _message;
        int _line_num;
};

class Config
{
    public:
        Config();
        explicit Config(const std::string& config_file);
        ~Config();

        void load(const std::string& config_file);
        const std::vector<ServerConfig>& getServers() const;

    private:
        std::vector<ServerConfig> _servers;
        std::string _config_file;
        int _current_line;

        /*Parsing*/
        void parseConfigFile(const std::string& filename);
        void parseServerBlock(std::ifstream& file, ServerConfig& server);
        void parseLocationBlock(std::ifstream& file, LocationConfig& location);

        /*Validation*/
        void validateBraces(const std::string& content);
        void validateServerBlock(const ServerConfig& server);
        void validateLocationBlock(const LocationConfig& location);
        void validatePort(int port);
        void validateMethod(const std::string& method);
        void validateErrorCode(int code);
        void validateBodySize(const std::string& size_str);
        void validatePath(const std::string& path);
        void checkDupDirective(const std::set<std::string>& seen, const std::string& directive);

        /*Utility*/
        std::string trim(const std::string& str);
        std::vector<std::string> split(const std::string& str, char delimiter);
        bool isValidInteger(const std::string& str);
        size_t parseBodySize(const std::string& size_str);
        std::string readEntireFile(const std::string& filename);
};

#endif

