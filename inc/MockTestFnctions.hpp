/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MockTestFnctions.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aistok <aistok@student.42london.com>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/11 12:49:26 by mosokina          #+#    #+#             */
/*   Updated: 2026/06/04 18:28:25 by aistok           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MOCKTESTFUNCTIONS_HPP
#define MOCKTESTFUNCTIONS_HPP

#include "Config.hpp"
#include "Listener.hpp"
#include "WebServ.hpp"

#include <iostream>
#include <csignal>

std::vector<ServerConfig> getMockConfig();
void runTemporaryTest(WebServ &ws);

#endif
