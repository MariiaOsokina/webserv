/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Version.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/20 21:56:26 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:22:31 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_VERSION_HPP
#define HTTP_VERSION_HPP

#include <string>

class HTTP_Version
{
public:
	static const std::string v1_1;
	static const std::string v1_0;

private:
	// Private and Unimplemented to prevent copying and class instantiation
	HTTP_Version();
	~HTTP_Version();
	HTTP_Version(const HTTP_Version &other);
	HTTP_Version &operator=(const HTTP_Version &other);
};

#endif // HTTP_VERSION_HPP
