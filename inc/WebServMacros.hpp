/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   WebServMacros.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/11 17:05:57 by aistok            #+#    #+#             */
/*   Updated: 2026/05/29 15:59:28 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_MACROS_HPP
#define WEBSERV_MACROS_HPP

#define ALLOWED_CHARS_IN_URI "-._~!$&'()*+,;=:@/?#" // RFC 3986 Section 2.2 & 2.3: pchar, unreserved, sub-delims

#define DEBUG_MODE 0
#define WEBSERV_NAME "miniMAA"
#define FILE_DUMPS_DIR "file_dumps"
#define DEFAULT_UPLOAD_FILENAME "uploaded_file"

#endif // WEBSERV_MACROS_HPP
