/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServ.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/14 19:03:57 by aistok            #+#    #+#             */
/*   Updated: 2026/06/06 21:04:09 by aistok           ###   ########.fr       */
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

#include "HTTP.hpp"
#include "Utils.hpp"
#include "Config.hpp"
#include "Listener.hpp"
#include "Connection.hpp"
#include "ErrorPages.hpp"
#include "CGI.hpp"

#include <poll.h> // For struct pollfd
#include <sys/types.h>
#include <sys/wait.h> // For waitpid and WNOHANG
#include <signal.h>	  // For kill and SIGKILL
#include <bits/types/sig_atomic_t.h>

#include <iostream>
#include <vector>
#include <map>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <cstdio> // For sprintf used to frame chunked-encoding headers
#include <ctime>
#include <string>

extern volatile sig_atomic_t g_server_running;

// Structs to track CGIs
struct CGIProcess
{
	pid_t pid;
	int stdoutFd;
	int stdinFd; // write end of body pipe, -1 once body fully sent / closed
	int connFd;
	std::string inputBody; // request body to drain into stdinFd
	size_t inputOffset;	   // bytes already written to stdinFd
	time_t startTime;

	// --- Streaming state (CGI stdout -> client socket) ---
	// headerBuf accumulates CGI output until the header/body boundary
	// (\r\n\r\n or \n\n); after that, body bytes stream straight to the
	// client. Capped at WebServ::MAX_CGI_HEADER_SIZE.
	std::string headerBuf;
	bool headersParsed;	 // headers parsed and prefix queued to client
	bool chunked;		 // wrap body in Transfer-Encoding: chunked
	bool headOnly;		 // suppress body (HEAD request)
	bool pollinPaused;	 // POLLIN removed for backpressure
	bool eofReached;	 // reserved, unused
	bool responseFailed; // reserved, unused

	CGIProcess()
		: pid(-1), stdoutFd(-1), stdinFd(-1), connFd(-1),
		  inputOffset(0), startTime(0),
		  headersParsed(false), chunked(false), headOnly(false),
		  pollinPaused(false), eofReached(false), responseFailed(false) {}
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
	void _readRequest(size_t *index);  // index -- if conn is closed
	void _sendResponse(size_t *index); // index -- if conn is closed

	void _checkConnTimeouts();

	// --- CGI Handler Methods ---
	bool _isCGIStdoutFd(int fd) const;
	bool _isCGIStdinFd(int fd) const;
	bool _executeCGI(int connFd, const std::string &cgiPath, const std::string &scriptPath);
	void _handleCGIOutput(size_t *index);
	void _handleCGIInput(size_t *index); // drains body to CGI stdin via POLLOUT
	// Closes stdin FD and unregisters it from poll/map state. If `cursor`
	// is given and the removed poll entry was at/before it, *cursor is
	// decremented to keep the surrounding loop index valid.
	void _closeCGIStdin(CGIProcess &cgi, size_t *cursor);
	void _checkCGITimeouts();
	void _cleanupCGI(int cgiFd);
	void _terminateCGIProcess(int cgiFd);

	// --- CGI streaming helpers ---
	// Parse the CGI header block from cgi.headerBuf. On success, fill the
	// response, queue the HTTP prefix + leftover body to conn, and switch
	// it to POLLOUT. Returns false if more bytes are needed; true once
	// done (a failure path already emits a 500).
	bool _tryParseCGIHeaders(CGIProcess &cgi, Connection *conn);
	// Append `n` body bytes to conn's stream buffer: chunked-framed
	// (HTTP/1.1) or raw (HTTP/1.0).
	void _appendBodyChunk(CGIProcess &cgi, Connection *conn,
						  const char *data, size_t n);
	// Sync client POLLOUT and CGI-stdout POLLIN to the buffer size
	// (back-pressure) after the stream buffer changes.
	void _syncStreamPollFlags(CGIProcess &cgi, Connection *conn);
	// Synthetic 500, only valid before any header bytes reached the client.
	void _sendCGIErrorResponse(int connFd);
	// Look up the poll index of an FD (returns SIZE_MAX if absent).
	size_t _findPollIndex(int fd) const;
	// Toggle event bits on a known FD (no-op if FD is not present).
	void _setPollEvents(int fd, short enable, short disable);
	// ----------------------------------

	static const int CONNECTION_TIMEOUT = 60;
	static const int POLL_TIMEOUT = 1000; // Wait up to 1 sec for events
	static const int CGI_TIMEOUT = 30;

	static const int BUFFER_SIZE = 4096;
	static const int CGI_BUFFER_SIZE = 8192;
	// CGI header section cap; larger is treated as malformed (500).
	static const size_t MAX_CGI_HEADER_SIZE = 8192;
	// Back-pressure on the forward buffer: stop reading CGI stdout above
	// HIGH_WATER, resume below LOW_WATER. Keeps per-connection RSS small.
	static const size_t CGI_FORWARD_HIGH_WATER = 65536;
	static const size_t CGI_FORWARD_LOW_WATER = 16384;
	std::vector<Listener *> _listeners;			// all server instances
	std::vector<pollfd> _pollFds;				// poll array for the whole program
	std::map<int, Listener *> _fdToListenerMap; // helps quickly find which server owns which FD
	std::map<int, Connection *> _fdToConnMap;

	std::map<int, CGIProcess> _cgiProcesses; // keyed by stdoutFd
	std::map<int, int> _cgiStdinToStdout;	 // stdinFd -> stdoutFd (for dispatch & cleanup)
};

#endif // WEBSERV_HPP
