/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServ.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mosokina <mosokina@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:03:57 by aistok            #+#    #+#             */
/*   Updated: 2026/05/26 19:33:37 by mosokina         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#define ESC_VIOLET_HOLLOW "\001\033[45;97m\002"
#define ESC_YELLOW_HOLLOW "\001\033[43;97m\002"
#define ESC_CYAN "\001\033[36m\002"
#define ESC_GREEN "\001\033[32m\002"
#define ESC_RED "\001\033[31m\002"
#define ESC_END "\001\033[0m\002"

#include <iostream>
#include <vector>
#include <map>
#include <poll.h> // For struct pollfd
#include <csignal>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <ctime>

#include "WebServMacros.hpp"
#include "Config.hpp"
#include "Listener.hpp"
#include "Connection.hpp"
#include "HTTP/HTTP.hpp"

extern volatile sig_atomic_t g_server_running;

/* Why Pointers <Listener *>:
 1. Memory Stability: std::vector reallocates memory as it grows. Using
 pointers ensures Listener objects stay at a fixed address, preventing
 dangling pointers in our _fdToServerMap or poll system.
 2. Polymorphism: Allows storing different types of Listener subclasses
 if the project expands.
 3. Efficiency: Avoids expensive "deep copies" of Listener objects
 during vector resizing.
 */


//Structs to track CGIs
struct CGIProcess {
    pid_t       pid;
    int         stdoutFd;
    int         stdinFd;        // write end of body pipe, -1 once body fully sent / closed
    int         connFd;
    std::string output;
    std::string inputBody;      // request body to drain into stdinFd
    size_t      inputOffset;    // bytes already written to stdinFd
    time_t      startTime;

    CGIProcess()
        : pid(-1), stdoutFd(-1), stdinFd(-1), connFd(-1),
          inputOffset(0), startTime(0) {}
};

class WebServ
{

public:
	WebServ();
	~WebServ();

	std::vector<Listener *> getListeners() const;
	void setup(const std::vector<ServerConfig> &configs);
	void run();

	// Connection &getConnectionForFd(int fd);

private:
	// Rule of Three: Private and Unimplemented
	WebServ(const WebServ &other);
	WebServ &operator=(const WebServ &other);

	void _addNewFdtoPool(int newFd, short events);
	void _updateEvent(size_t index, short enable, short disable);

	bool _isListener(int fd);
	void _acceptNewConnection(int listenFd);
	void _closeConnection(size_t index);
	void _readRequest(size_t *index); // index -- if conn is closed 
	void _sendResponse(size_t *index); //index -- if conn is closed

	void _checkConnTimeouts();

	// --- CGI Handler Methods ---
    bool _isCGISocket(int fd) const;        // true if fd is a CGI stdout (read) FD
    bool _isCGIStdinFd(int fd) const;       // true if fd is a CGI stdin (write) FD
    bool _executeCGI(int connFd, const std::string& cgiPath, const std::string& scriptPath);
    void _handleCGIOutput(size_t *index);
    void _handleCGIInput(size_t *index);    // drains body to CGI stdin via POLLOUT
    // Closes stdin FD and unregisters it from poll/map state. If `cursor`
    // is provided and the removed poll entry was at or before *cursor,
    // *cursor is decremented so the surrounding loop stays consistent.
    void _closeCGIStdin(CGIProcess& cgi, size_t *cursor);
    void _checkCGITimeouts();
    void _cleanupCGI(int cgiFd);
    void _closeCGIPipe(size_t index);
	void _terminateCGIProcess(int cgiFd); //MO: added
    // ----------------------------------
	
	static const int CONNECTION_TIMEOUT = 60; //sec TO-DO change to 60 sec(most common default in ngenx) or parse from confif 
	static const int POLL_TIMEOUT = 1000;	// Wait up to 1 sec for events
	static const int CGI_TIMEOUT = 30;

	static const int BUFFER_SIZE = 4096;
	static const int CGI_BUFFER_SIZE = 8192;
	std::vector<Listener *> _listeners;			// all server instances
	std::vector<pollfd> _pollFds;			// poll array for the whole program
	std::map<int, Listener *> _fdToListenerMap; // helps quickly find which server owns which FD
	std::map<int, Connection *> _fdToConnMap;

    std::map<int, CGIProcess> _cgiProcesses;        // keyed by stdoutFd
    std::map<int, int>        _cgiStdinToStdout;    // stdinFd -> stdoutFd (for dispatch & cleanup)
};

#endif // WEBSERV_HPP
