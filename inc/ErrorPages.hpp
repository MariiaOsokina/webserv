/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorPages.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/25 08:52:13 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 18:27:18 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ERROR_PAGES_HPP
#define ERROR_PAGES_HPP

#include "HTTP/HTTP_Status.hpp"
#include "Utils.hpp"
#include "Config.hpp"

#include <string>

struct ServerConfig;

class ErrorPages
{
public:
	ErrorPages();
	~ErrorPages();

	static std::string generate(const HTTP_StatusPair &status);
	static std::string getContent(const ServerConfig &sc, const HTTP_StatusPair &status);

private:
	static std::string _template;
};

#endif // ERROR_PAGES_HPP
