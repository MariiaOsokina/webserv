/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/26 23:56:58 by mosokina          #+#    #+#             */
/*   Updated: 2026/06/04 18:28:39 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

#define FORBID_NEGATIVES true

#include "WebServMacros.hpp"

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h> // used for stat
#include <unistd.h>	  // stat, access

#include <cstdlib> // for NULL
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

template <typename T>
std::string toString(const T &value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

template <typename T>
bool toNumber(const std::string &str, T &out, bool forbidNegatives = false)
{
	if (forbidNegatives)
	{
		size_t pos = 0;
		while (pos < str.size())
		{
			if (std::isspace(str[pos]) || !std::isdigit(str[pos]))
				return (false);
			if (std::isdigit(str[pos]))
				break;
			++pos;
		}
	}

	std::istringstream iss(str);
	iss >> out;

	return !iss.fail() && iss.eof();
}

bool setNonBlocking(int fd);

std::string &capitaliseFirstLetters(std::string &str);
bool replace(std::string &str, const std::string &from, const std::string &to);

enum PathType
{
	PATH_NONE,
	PATH_FILE,
	PATH_DIRECTORY
};

// file system item
typedef struct fsItem
{
	//	std::string path;
	std::string name;
	size_t size;
	bool isDir;
	bool isReadable;
	//	bool isWritable;

	fsItem() : size(0), isDir(false), isReadable(false) {}
} fsItem;

bool removeLastPortion(std::string &line, const std::string &portion);

PathType getPathType(const std::string &pathStr);

class Utils
{
public:
	/*String Manipulations*/
	static std::string trim(const std::string &str);
	static std::string &trim(std::string &str, const std::string &stripChars);
	static std::string toLowerCase(const std::string &str);
	static std::string toUpperCase(const std::string &str);
	static std::vector<std::string> split(const std::string &str, char delimiter);
	static std::vector<std::string> split(const std::string &str, const std::string &delimiter);
	static bool startsWith(const std::string &str, const std::string &prefix);
	static bool endsWith(const std::string &str, const std::string &suffix);
	static std::string replaceAll(const std::string &src,
								  const std::string &from,
								  const std::string &to);
	static std::string removeQuote(const std::string &value, const char quote);
	static std::size_t countOccurrence(const std::string &haystack, const std::string &needle);
	static std::string substrUpTo(const std::string &str, const std::string &needle);

	/*File operations*/
	static bool fileExists(const std::string &path);
	// static std::string readFile(const std::string &path);
	// static bool writeFile(const std::string &path, const std::string &content);
	static bool deleteFile(const std::string &path);
	static std::string getFileContent(const std::string &filename);
	static std::vector<fsItem> getDirectoryList(const std::string &path);
	static bool isReadable(const std::string &pathOnServer);
	static bool isWritable(const std::string &pathOnServer);
	static std::string getNextAvailableFilename(const std::string &file_name_with_extension);
	static bool writeStringToFile(const std::string &filename_with_extension, const std::string &data);
	static std::string getcwd();
	static std::string dumpToFile(const std::string &filename, const std::string &data);

	/*Path operations*/
	static std::string joinPath(const std::string &base, const std::string &relative);
	static std::string getAbsolutePath(const std::string &relativePath);
	static std::string normalizePath(const std::string &path);
	static std::string getExtension(const std::string &path);
	static std::string getFileName(const std::string &path);
	static std::string getDirectory(const std::string &path);

	/*MIME types*/
	static std::string getMimeType(const std::string &extension);

	/*URL operations*/
	static std::string urlDecode(const std::string &str);
	static std::string urlEncode(const std::string &str);
	static bool isValidPercentEncoded(const std::string &str, size_t pos);
	static bool isValidUriChar(char c);

	/*Number conversions*/
	static std::string toString(int n);
	static std::string toString(size_t n);
	static int toInt(const std::string &str);
	static size_t toSizeT(const std::string &str);

	/*Time*/
	static std::string getHttpDate();

private:
	Utils();
	~Utils();
	Utils(const Utils &);
	Utils &operator=(const Utils &);
};

#endif
