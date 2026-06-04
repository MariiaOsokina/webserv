/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/25 11:32:29 by mosokina          #+#    #+#             */
/*   Updated: 2026/06/04 18:50:14 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "WebServMacros.hpp"
#include "Utils.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>
#include <iterator>
#include <fstream>
#include <stdexcept>
#include <ctime>

/*
 * FD_CLOEXEC: Prevents File Descriptor "leaks" across processes.
 * When a child process calls execve(), any FD with this flag is
 * automatically closed, ensuring CGI scripts don't accidentally
 * inherit open pipes or sockets belonging to other clients.
 * the door shut on every single fd that doesn't explicitly belong to that specific child.
 */

bool setNonBlocking(int fd)
{
	if (fd < 0)
		return false;
	// 1. Handle File Status Flags (O_NONBLOCK)
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return false;
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		return false;
	// 2. Handle File Descriptor Flags (FD_CLOEXEC)
	int fd_flags = fcntl(fd, F_GETFD, 0);
	if (fd_flags != -1)
		fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC);
	return true;
}

std::string &capitaliseFirstLetters(std::string &str)
{
	unsigned char previousChar;
	unsigned char firstChar;
	unsigned char currentChar;

	if (!str.empty())
	{
		for (std::string::size_type i = 0; i < str.size(); ++i)
		{
			if (i > 0)
				previousChar = static_cast<unsigned char>(str[i - 1]);

			if (i == 0 || (i > 0 && !std::isalpha(previousChar)))
			{
				firstChar = static_cast<unsigned char>(str[i]);
				str[i] = static_cast<char>(std::toupper(firstChar));
			}
			else
			{
				currentChar = static_cast<unsigned char>(str[i]);
				str[i] = static_cast<char>(std::tolower(currentChar));
			}
		}
	}
	return (str);
}

bool replace(std::string &str, const std::string &what, const std::string &with)
{
	size_t start_pos = str.find(what);
	if (start_pos == std::string::npos)
		return false;
	str.replace(start_pos, what.length(), with);
	return true;
}

bool removeLastPortion(std::string &line, const std::string &portion)
{
	if (line.empty() || portion.empty())
		return (false);

	if (Utils::endsWith(line, portion))
	{
		line.erase(line.length() - portion.length(), portion.length());
		return (true);
	}

	return (false);
}

PathType getPathType(const std::string &pathStr)
{
	struct stat st;

	if (stat(pathStr.c_str(), &st) != 0)
		return PATH_NONE;

	if (S_ISREG(st.st_mode))
		return PATH_FILE;

	if (S_ISDIR(st.st_mode))
		return PATH_DIRECTORY;

	return PATH_NONE;
}

/*private constructors to prevent instantiation*/
Utils::Utils() {}
Utils::~Utils() {}

std::string Utils::trim(const std::string &str)
{
	size_t start = 0;
	size_t end = str.length();

	while (start < end && std::isspace(str[start]))
		++start;
	while (end > start && std::isspace(str[end - 1]))
		--end;
	return (str.substr(start, end - start));
}

/* this function will modify the string itself! */
std::string &Utils::trim(std::string &str, const std::string &stripChars)
{
	std::string::size_type start = 0;
	std::string::size_type end = str.size();

	if (end > 0)
	{
		while (start < end &&
			   (stripChars.find(str[start]) != std::string::npos))
			++start;

		while (end > start &&
			   (stripChars.find(str[end - 1]) != std::string::npos))
			--end;

		if (start != 0 || end != str.size())
			str = str.substr(start, end - start);
	}
	return (str);
}

std::string Utils::toLowerCase(const std::string &str)
{
	std::string res = str;

	for (size_t i = 0; i < str.length(); ++i)
		res[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(res[i])));

	return (res);
}

std::string Utils::toUpperCase(const std::string &str)
{
	std::string res = str;

	for (size_t i = 0; i < str.length(); ++i)
		res[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(res[i])));

	return (res);
}

std::vector<std::string> Utils::split(const std::string &str, char delimiter)
{
	std::vector<std::string> res;
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, delimiter))
	{
		if (!item.empty())
			res.push_back(item);
	}

	return (res);
}

std::vector<std::string> Utils::split(const std::string &str, const std::string &delimiter)
{
	std::vector<std::string> res;
	size_t start = 0;
	size_t end = str.find(delimiter);

	while (end != std::string::npos)
	{
		res.push_back(str.substr(start, end - start));
		start = end + delimiter.length();
		end = str.find(delimiter, start);
	}
	res.push_back(str.substr(start));

	return (res);
}

bool Utils::startsWith(const std::string &str, const std::string &prefix)
{
	if (prefix.length() > str.length())
		return (false);
	return (str.substr(0, prefix.length()) == prefix);
}

bool Utils::endsWith(const std::string &str, const std::string &suffix)
{
	if (suffix.length() > str.length())
		return (false);
	return (str.substr(str.length() - suffix.length()) == suffix);
}

std::string Utils::replaceAll(const std::string &src,
							  const std::string &what,
							  const std::string &with)
{
	if (what.empty())
		return src;

	std::string result = src;
	std::string::size_type pos = 0;

	while ((pos = result.find(what, pos)) != std::string::npos)
	{
		result.replace(pos, what.length(), with);
		pos += with.length();
	}
	return result;
}

std::string Utils::removeQuote(const std::string &value, const char quote)
{
	if (value.size() >= 2 &&
		value[0] == quote && value.find_last_of(quote) == value.size() - 1)
		return (value.substr(1, value.size() - 2));

	return (value);
}

std::size_t Utils::countOccurrence(const std::string &haystack, const std::string &needle)
{
	if (needle.empty())
		return (0);

	size_t occurrence = 0;
	size_t pos = haystack.find(needle);

	while (pos != std::string::npos)
	{
		++occurrence;
		pos = haystack.find(needle, pos + needle.size());
	}

	return (occurrence);
}

std::string Utils::substrUpTo(const std::string &str, const std::string &needle)
{
	size_t needle_pos = str.find(needle);
	if (needle_pos == std::string::npos)
		return ("");

	return (str.substr(0, needle_pos));
}

bool Utils::fileExists(const std::string &path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0);
}

bool Utils::deleteFile(const std::string &path)
{
	return (unlink(path.c_str()) == 0);
}

/*Number conversions*/
int Utils::toInt(const std::string &str)
{
	std::stringstream ss(str);
	int res;
	ss >> res;
	return (res);
}

size_t Utils::toSizeT(const std::string &str)
{
	std::stringstream ss(str);
	size_t res;
	ss >> res;
	return (res);
}

std::string Utils::getFileContent(const std::string &filename)
{
	std::ifstream file(filename.c_str(), std::ios_base::binary);

	if (!file.is_open())
	{
		throw std::runtime_error(filename + " can not be opened!");
	}

	std::string content((std::istreambuf_iterator<char>(file)),
						std::istreambuf_iterator<char>());

	return content;
}

std::vector<fsItem> Utils::getDirectoryList(const std::string &path)
{
	std::vector<fsItem> result;

	DIR *dir = opendir(path.c_str());
	if (!dir)
		throw std::runtime_error(path + " is not a directory!");

	struct dirent *entry;
	struct stat st;

	while ((entry = readdir(dir)) != NULL)
	{
		fsItem item;

		item.name = entry->d_name;
		if (item.name != "." && item.name != "..")
		{
			std::string itemPath = path + "/" + item.name;
			if (stat(itemPath.c_str(), &st) == 0)
			{
				item.isReadable = Utils::isReadable(itemPath);
				item.isDir = (st.st_mode & S_IFDIR) != 0;
				if (!item.isDir)
					item.size = st.st_size;
			}
			else
			{
				// ... this should not happen - the way this function is called
			}
			result.push_back(item);
		}
	}

	closedir(dir);
	// std::sort(result.begin(), result.end());

	return (result);
}

bool Utils::isReadable(const std::string &pathOnServer)
{
	return access(pathOnServer.c_str(), R_OK) == 0;
}

bool Utils::isWritable(const std::string &pathOnServer)
{
	return access(pathOnServer.c_str(), W_OK) == 0;
}

std::string Utils::getNextAvailableFilename(const std::string &file_name_with_extension)
{
	std::stringstream ss;
	int counter = 1;

	std::string file_dir = Utils::getDirectory(file_name_with_extension);
	std::string file_name = Utils::getFileName(file_name_with_extension);
	std::string file_ext = Utils::getExtension(file_name);

	file_name = file_name.substr(0, file_name.length() - file_ext.length());

	std::string filename = file_dir + '/' + file_name + file_ext;

	while (true)
	{
		if (!fileExists(filename))
			break;

		ss.str("");
		ss << file_dir << '/' << counter << "_" << file_name << file_ext;
		filename = ss.str();

		++counter;
	}

	return (filename);
}

bool Utils::writeStringToFile(const std::string &filename_with_extension, const std::string &data)
{
	std::ofstream outfile(filename_with_extension.c_str(), std::ios::binary);
	if (!outfile)
		return (false);

	outfile.write(data.data(), data.size());
	outfile.close();
	return (true);
}

std::string Utils::getcwd()
{
	char buffer[1024];

	if (::getcwd(buffer, sizeof(buffer)) != NULL)
		return std::string(buffer);
	else
		return "";
}

std::string Utils::dumpToFile(const std::string &filename, const std::string &data)
{
	std::string filename_to_check;
	filename_to_check.append(FILE_DUMPS_DIR).append("/").append(filename);

	std::string filename_ok = Utils::getNextAvailableFilename(filename_to_check);
	Utils::writeStringToFile(filename_ok, data);

	return (filename_ok);
}

// returns extension with the dot, ex: .txt
std::string Utils::getExtension(const std::string &path)
{
	if (path.empty())
		return ("");

	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos || dot == path.length() - 1)
	{
		return "";
	}
	return (path.substr(dot));
}

/*Path operations*/
std::string Utils::joinPath(const std::string &base, const std::string &relative)
{
	if (base.empty())
	{
		return (relative);
	}

	if (relative.empty())
	{
		return (base);
	}

	if (base[base.length() - 1] == '/')
	{
		if (relative[0] == '/')
		{
			return (base + relative.substr(1));
		}
		else if (Utils::startsWith(relative, "./"))
		{
			return (base + relative.substr(2));
		}
		return (base + relative);
	}
	else
	{
		if (relative[0] == '/')
		{
			return (base + relative);
		}
		else if (Utils::startsWith(relative, "./"))
		{
			return (base + "/" + relative.substr(2));
		}
		return (base + "/" + relative);
	}
}

// will append the relative path to the current working directory
//
// NOTE: it is important, that the current working directory and the
//       relative path match;
//
//       ex: if the relative path is a valid path in the current
//           working directory and the directory is changed to some
//           other directory, this function will return an eronneous
//           absolute path!
std::string Utils::getAbsolutePath(const std::string &relativePath)
{
	if (relativePath[0] != '/')
	{
		std::string cwd = Utils::getcwd();
		return (Utils::joinPath(cwd, relativePath));
	}

	return (relativePath);
}

std::string Utils::normalizePath(const std::string &path)
{
	std::vector<std::string> parts = split(path, '/');
	std::vector<std::string> result;

	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (parts[i] == "..")
		{
			if (!result.empty())
			{
				result.pop_back();
			}
		}
		else if (parts[i] != ".")
		{
			result.push_back(parts[i]);
		}
	}

	std::string normalized;
	if (path[0] == '/')
	{
		normalized = "/";
	}
	for (size_t i = 0; i < result.size(); ++i)
	{
		if (i > 0)
		{
			normalized += "/";
		}
		normalized += result[i];
	}

	return (normalized.empty() ? "/" : normalized);
}

// returns filename with extension, ex: test.jpg
std::string Utils::getFileName(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
	{
		return (path);
	}

	return (path.substr(slash + 1));
}

// returns path, cutting off everything from the last '/'
// example:
// /path/to/file -> /path/to
// /path/to/dir -> /path/to
std::string Utils::getDirectory(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
	{
		return (".");
	}
	if (slash == 0)
	{
		return ("/");
	}

	return (path.substr(0, slash));
}

/*Mime types*/
std::string Utils::getMimeType(const std::string &extension)
{
	std::string ext = toLowerCase(extension);

	if (ext == ".html" || ext == ".htm")
		return ("text/html");
	if (ext == ".css")
		return ("text/css");
	if (ext == ".js")
		return ("application/javascript");
	if (ext == ".json")
		return ("application/json");
	if (ext == ".xml")
		return ("application/xml");
	if (ext == ".jpg" || ext == ".jpeg")
		return ("image/jpeg");
	if (ext == ".png")
		return ("image/png");
	if (ext == ".gif")
		return ("image/gif");
	if (ext == ".svg")
		return ("image/svg+xml");
	if (ext == ".ico")
		return ("image/x-icon");
	if (ext == ".pdf")
		return ("application/pdf");
	if (ext == ".txt")
		return ("text/plain");
	if (ext == ".zip")
		return ("application/zip");
	if (ext == ".tar")
		return ("application/x-tar");
	if (ext == ".gz")
		return ("application/gzip");

	return "application/octet-stream";
}

std::string Utils::urlDecode(const std::string &input)
{
	std::string res;
	if (input.size() < 3)
		return (res = input);

	for (size_t i = 0; i < input.length(); ++i)
	{
		if (input[i] == '%' &&
			isValidPercentEncoded(input, i))
		{

			int value = 0;

			std::stringstream ss;
			ss << std::hex << input.substr(i + 1, 2);
			ss >> value;

			res += static_cast<char>(value);
			i += 2;
		}
		else if (input[i] == '+')
		{
			res += ' ';
		}
		else
		{
			res += input[i];
		}
	}

	return (res);
}

std::string Utils::urlEncode(const std::string &str)
{
	std::ostringstream oss;

	for (size_t i = 0; i < str.length(); ++i)
	{
		char c = str[i];
		if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			oss << c;
		}
		else
		{
			oss << '%'
				<< std::uppercase << std::setw(2) << std::setfill('0')
				<< std::hex << (int)(unsigned char)c
				<< std::nouppercase;
		}
	}
	return (oss.str());
}

bool Utils::isValidPercentEncoded(const std::string &str, size_t pos)
{
	if (pos + 2 >= str.length())
		return (false);

	char h1 = str[pos + 1];
	char h2 = str[pos + 2];
	return (std::isxdigit(static_cast<unsigned char>(h1)) &&
			std::isxdigit(static_cast<unsigned char>(h2)));
}

bool Utils::isValidUriChar(char c)
{
	const static std::string allowed(ALLOWED_CHARS_IN_URI);

	unsigned char uc = static_cast<unsigned char>(c);
	if (std::isalnum(uc))
		return (true);

	return (allowed.find(c) != std::string::npos);
}

std::string Utils::toString(int n)
{
	std::ostringstream oss;
	oss << n;
	return (oss.str());
}

std::string Utils::toString(size_t n)
{
	std::ostringstream oss;
	oss << n;
	return (oss.str());
}

/*Time*/
std::string Utils::getHttpDate()
{
	time_t now = time(NULL);
	struct tm *gmt = gmtime(&now);

	char buffer[100];
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);

	return (std::string(buffer));
}
