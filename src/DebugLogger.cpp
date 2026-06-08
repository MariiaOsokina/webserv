/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DebugLogger.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/07 17:18:25 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:54:03 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "DebugLogger.hpp"

#if defined(DEBUG_MODE) && DEBUG_MODE == 1

DebugLogger::DebugLogger(std::ostream &os = std::cout) : _stream(os) {}

DebugLogger::~DebugLogger() {}

#else

DebugLogger::DebugLogger(std::ostream &) {}

DebugLogger::~DebugLogger() {}

DebugLogger::DebugLogger() {}
inline DebugLogger &DebugLogger::operator=(const DebugLogger &)
{
	return (*this);
}
#endif // DEBUG
