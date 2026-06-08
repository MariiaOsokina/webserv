/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DebugLogger.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/07 17:18:25 by aistok            #+#    #+#             */
/*   Updated: 2026/06/08 12:55:23 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef DebugLogger_HPP
#define DebugLogger_HPP

#include <iostream>
#include "WebServMacros.hpp"

#if defined(DEBUG_MODE) && DEBUG_MODE == 1

class DebugLogger
{
public:
	explicit DebugLogger(std::ostream &os);

	template <typename T>
	DebugLogger &operator()(const T &val)
	{
		_stream << val;
		return *this;
	}

	~DebugLogger();

private:
	std::ostream &_stream;

	DebugLogger();
	DebugLogger(const DebugLogger &other);
	DebugLogger &operator=(const DebugLogger &other);
};

#else

class DebugLogger
{
public:
	explicit DebugLogger(std::ostream &);

	// Inline template absorbs any argument and returns a reference to itself.
	// The compiler optimizes this away into absolutely nothing.
	template <typename T>
	inline DebugLogger &operator()(const T &)
	{
		return *this;
	}

	// Empty destructor
	~DebugLogger();

private:
	// Rule of Three: Private and Unimplemented to prevent copying
	DebugLogger();
	DebugLogger(const DebugLogger &other);
	DebugLogger &operator=(const DebugLogger &other);
};

#endif // DEBUG

#endif // LOGGER_HPP
