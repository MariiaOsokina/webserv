/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DirectoriesToHTML.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/25 08:52:13 by aistok            #+#    #+#             */
/*   Updated: 2026/06/04 18:27:06 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef DIRECTORIES_TO_HTML_HPP
#define DIRECTORIES_TO_HTML_HPP

#include "Utils.hpp"

#include <string>
#include <vector>

class DirectoriesToHTML
{
public:
	DirectoriesToHTML();
	~DirectoriesToHTML();

	static std::string generate(const std::vector<fsItem> &itemsList,
								const std::string &requestURL,
								const bool includeDeleteButton);

private:
	static std::string _templateHTMLPage;
	static std::string _templateHTMLDirectoryListItem;
	static std::string _templateDeleteJavaScript;
	static std::string _templateDeleteButton;
};

#endif // DIRECTORIES_TO_HTML_HPP
