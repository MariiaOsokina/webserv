/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HTTP_Method.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/20 21:56:26 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:21:38 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_METHOD_HPP
#define HTTP_METHOD_HPP

#include <string>

class HTTP_Method
{
public:
	static const std::string GET;
	static const std::string HEAD;
	static const std::string POST;
	static const std::string DELETE;

private:
	// Private and Unimplemented to prevent copying
	HTTP_Method();
	~HTTP_Method();
	HTTP_Method(const HTTP_Method &other);
	HTTP_Method &operator=(const HTTP_Method &other);
};

#endif // HTTP_METHOD_HPP
