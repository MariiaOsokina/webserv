/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:53:52 by aaladeok          #+#    #+#             */
/*   Updated: 2026/05/14 22:30:23 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Config.hpp"
#include "Utils.hpp" //-->Needs implementation.
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cctype>

/*ConfigException class methods*/
ConfigException::ConfigException(const std::string& msg, int line) 
    : _message(msg), _line_num(line) {
    if (line >= 0) {
        std::ostringstream oss;
        oss << "Config error at line " << line << ": " << msg;
        _message = oss.str();
    }
}

ConfigException::~ConfigException() throw() {}

const char* ConfigException::what() const throw() {
    return (_message.c_str());
}

int ConfigException::getLineNumber() const {
    return (_line_num);
}

/*Config constuctors & methods*/
Config::Config() : _current_line(0) {}

Config::Config(const std::string& config_file) : _config_file(config_file) {
    load(config_file);
}

Config::~Config() {}

void Config::load(const std::string& config_file) {
    _config_file = config_file;
    _current_line = 0;
    _servers.clear();

    /*Read entire config file into memory for brace validation*/
    std::string content = readEntireFile(config_file);
    validateBraces(content);

    parseConfigFile(config_file);
}

std::string Config::trim(const std::string& str) {
    return (Utils::trim(str));
}

std::vector<std::string> Config::split(const std::string& str, char delimiter) {
    return (Utils::split(str, delimiter));
}

std::string Config::readEntireFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw ConfigException("Cannot open config file: " + filename);
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/*Validation methods*/
void Config::validateBraces(const std::string& content) {
    int brace_count = 0;
    int line = 1;
    bool in_string = false;
    char string_delimiter = 0;

    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];

        if (c == '\n')
            line++;
        /*Handling string that start/end */
        if (!in_string && (c == '"' || c == '\'')) {
            in_string = true;
            string_delimiter = c;
            continue;
        }
        else if (in_string && c == string_delimiter) {
            size_t backslash_count = 0;
            size_t j = i;

            while (j > 0 && content[--j] == '\\')
                backslash_count++;

            if (backslash_count % 2 == 0) {
                in_string = false;
                string_delimiter = 0;
            }
            continue;
        }

        if (in_string)
            continue;
        /*Skip comments*/
        if (c == '#') {
            while (i < content.length() && content[i] != '\n')
                i++;
            continue;
        }
        if (c == '{')
            brace_count++;
        else if (c == '}') {
            brace_count--;
            if (brace_count < 0) {
                throw ConfigException("Unexpected closing brace", line);
            }
        }
    }

    if (brace_count != 0) {
        throw ConfigException("Mismatched braces in config file");
    }
}

bool Config::isValidInteger(const std::string& str) {
    if (str.empty()) 
        return (false);

    for (size_t i = 0; i < str.length(); ++i) {
        if (!std::isdigit(str[i]))
            return (false);
    }
    return (true);
}

void Config::validatePort(int port) {
    if (port < 1 || port > 65535) {
        throw ConfigException("Invalid port number (must be between 1 - 65535)", _current_line);
    }
}

void Config::validateMethod(const std::string& method) {
    if (method != "HEAD" && method != "GET" && method != "POST" && method != "DELETE") {
        throw ConfigException("Invalid HTTP method: " + method + " (allowed: GET, POST, DELETE)", _current_line);
    }
}

void Config::validateErrorCode(int code) {
    //Common HTTP error codes
    if (code < 100 || code > 599)
        throw ConfigException("Invalid HTTP error code", _current_line);
}

void Config::validateBodySize(const std::string& size_str) {
    if (size_str.empty())
        throw ConfigException("Empty body size value", _current_line);

    std::string num_part = size_str;
    char unit = 0;

    if (!std::isdigit(size_str[size_str.length() - 1])) {
        unit = size_str[size_str.length() - 1];
        num_part = size_str.substr(0, size_str.length() - 1);

        if (unit != 'K' && unit != 'k' && unit != 'M' && unit != 'm' && 
            unit != 'G' && unit != 'g') {
            throw ConfigException("Invalid body size unit (use K, M, or G)", _current_line);
        }
    }

    if (!isValidInteger(num_part)) {
        throw ConfigException("Invalid body size number", _current_line);
    }
}

void Config::validatePath(const std::string& path) {
    if (path.empty())
        throw ConfigException("Empty path", _current_line);

    //Paths should start with /
    if (path[0] != '/' && path[0] != '.') {
        throw ConfigException("Path must start with a '/' or '.'", _current_line);
    }
}

void Config::checkDupDirective(const std::set<std::string>& seen, const std::string& directive) {
    if (seen.find(directive) != seen.end()) {
        throw ConfigException("Duplicate directive: " + directive, _current_line);
    }
}

void Config::validateServerBlock(const ServerConfig& server) {
    //At least one port is required
    if (server.ports.empty()) {
        throw ConfigException("Server block must have at least one 'listen' directive");
    }

    //Validating port uniqueness within the server
    std::set<int> port_set;
    for (size_t i = 0; i < server.ports.size(); ++i) {
        if (port_set.find(server.ports[i]) != port_set.end()) {
            std::ostringstream oss;
            oss << "Duplicate port " << server.ports[i] << " already in server block";
            throw ConfigException(oss.str());
        }
        port_set.insert(server.ports[i]);
    }
}

void Config::validateLocationBlock(const LocationConfig& location) {
    //Path is required and already set
    if (location.path.empty()) {
        throw ConfigException("Location requires a path");
    }

    //If redirect is set, other directives should be minimal
    if (!location.redirect_url.empty() && !location.root.empty()) {
        throw ConfigException("Location with 'return' should not have 'root'");
    }

    //If root and alias are exclusive of each other
    if (!location.root.empty() && !location.alias.empty()) {
        throw ConfigException("Location should have either 'root' or 'alias'");
    }
}

/*Parsing methods*/
size_t Config::parseBodySize(const std::string& size_str) {
    validateBodySize(size_str);

    size_t multiplier = 1;
    std::string num_str = size_str;

    if (!size_str.empty() && !std::isdigit(size_str[size_str.length() - 1])) {
        char last = size_str[size_str.length() - 1];
        num_str = size_str.substr(0, size_str.length() - 1);

        if (last == 'K' || last == 'k') {
            multiplier = 1024;
        } else if (last == 'M' || last == 'm') {
            multiplier = 1024 * 1024;
        } else if (last == 'G' || last == 'g') {
            multiplier = 1024 * 1024 * 1024;
        }
    }
    return (Utils::toSizeT(num_str) * multiplier);
}

void Config::parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw ConfigException("Cannot open config file: " + filename);
    }

    std::string line;
    _current_line = 0;

    while (std::getline(file, line)) {
        _current_line++;
        line = Utils::trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line == "server {") {
            ServerConfig server;
            parseServerBlock(file, server);
            validateServerBlock(server);
            _servers.push_back(server);
        } else {
            throw ConfigException("Unexpected directive outside server block: " + line,
                                 _current_line);
        }
    }

    if (_servers.empty()) {
        throw ConfigException("No server blocks found in config file");
    }
    file.close();
}

void Config::parseServerBlock(std::ifstream& file, ServerConfig& server) {
    std::string line;
    std::set<std::string> seen_directives;

    while(std::getline(file, line)) {
        _current_line++;
        line = trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line == "}")
            return;

        /*Handling location block*/
        if (line.find("location") == 0) {
            //we parse -> "location /path {"
            std::vector<std::string> parts = split(line, ' ');
            if (parts.size() < 2 ) {
                throw ConfigException("Invalid location directive", _current_line);
            }

            std::string path = parts[1];
            //checking location of open brace current or next line
            if (parts.size() > 2 && parts[2] == "{") {
                //braces on same line..Here we do nothing.
            } else {
                //We read the next line for brace
                std::string next_line;
                if (!std::getline(file, next_line)) {
                    throw ConfigException("Unexpected end of file after location", _current_line);
                }
                _current_line++;
                next_line = trim(next_line);
                if (next_line != "{") {
                    throw ConfigException("Expected '{' after location path", _current_line);
                }
            }

            LocationConfig location;
            location.path = path;
            parseLocationBlock(file, location);
            validateLocationBlock(location);
            server.locations.push_back(location);
            continue;
        }

        //Parsing regular directive
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty())
            continue;

        std::string key = tokens[0];

        //Getting rid of trailing semicolon
        size_t semicolon = line.find(';');
        if (semicolon == std::string::npos) {
            throw ConfigException("Missing semicolon at the end of directive" + line, _current_line);
        }

        std::string value = trim(line.substr(key.length(), semicolon - key.length()));

        if (key == "listen" || key == "port") {
            if (!isValidInteger(value)) {
                throw ConfigException("Invalid port number: " + value, _current_line);
            }
            int port = Utils::toInt(value);
            validatePort(port);
            server.ports.push_back(port);

        } else if (key == "host") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            server.host = value;

        } else if (key == "server_name") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            server.server_names = split(value, ' ');

        } else if (key == "error_page") {
            std::vector<std::string> parts = split(value, ' ');
            if (parts.size() != 2) {
                throw ConfigException("error_page requires error code and path", _current_line);
            }
            if (!isValidInteger(parts[0])) {
                throw ConfigException("Invalid error code: " + parts[0], _current_line);
            }
            int code = Utils::toInt(parts[0]);
            validateErrorCode(code);
            server.error_pages[code] = parts[1];

        } else if (key == "client_max_body_size") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            server.client_max_body_size = parseBodySize(value);

        }
        else if (key == "root") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            validatePath(value);
            server.root = value;

        }
        else if (key == "index") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            server.index = value;
        } else {
            throw ConfigException("Unknown server directive: " + key, _current_line);
        }
    }

    throw ConfigException("Unexpected end of file (missing closing brace)");
}

void Config::parseLocationBlock(std::ifstream& file, LocationConfig& location) {
    std::string line;
    std::set<std::string> seen_directives;

    while (std::getline(file, line)) {
        _current_line++;
        line = trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        if (line == "}")
            return;

        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty())
            continue;

        std::string key = tokens[0];
        size_t semicolon = line.find(';');
        if (semicolon == std::string::npos) {
            throw ConfigException("Missing semicolon at the end of directive: " + line, _current_line);
        }

        std::string value = trim(line.substr(key.length(), semicolon - key.length()));

        if (key == "allowed_methods") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);

            std::vector<std::string> methods = split(value, ' ');
            for (size_t i = 0; i < methods.size(); ++i) {
                validateMethod(methods[i]);
            }
            location.allowed_methods = methods;

        } else if (key == "root") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            validatePath(value);
            location.root = value;

        } else if (key == "alias") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            validatePath(value);
            location.alias = value;

        } else if (key == "autoindex") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            if (value != "on" && value != "off") {
                throw ConfigException("autoindex must be 'on' or 'off'", _current_line);
            }
            location.autoindex = (value == "on");

        } else if (key == "index") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            location.index = value;

        } else if (key == "return") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            location.redirect_url = value;

        } else if (key == "upload_path") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            validatePath(value);
            location.upload_path = value;

        } else if (key == "cgi_pass") {
            std::vector<std::string> parts = split(value, ' ');
            if (parts.size() != 2) {
                throw ConfigException("cgi_pass requires extension and path", _current_line);
            }
            if (parts[0].empty() || parts[0][0] != '.') {
                throw ConfigException("CGI extension must start with '.'", _current_line);
            }
            location.cgi_extensions[parts[0]] = parts[1];

        } else if (key == "client_max_body_size") {
            checkDupDirective(seen_directives, key);
            seen_directives.insert(key);
            location.client_max_body_size = parseBodySize(value);

        } else {
            throw ConfigException("Unknown location directive: " + key, _current_line);
        }
    }

    throw ConfigException("Unexpected end of file in location block");
}

/*Getters*/
const std::vector<ServerConfig>& Config::getServers () const {
    return (_servers);
}
