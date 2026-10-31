// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Command-line Interface
//

#pragma once


// Standard libs
#include <string>
#include <exception>


class mps_error: public std::exception {
public:
	std::string msg;
	mps_error(const std::string& msg = ""): msg(msg) {}

	virtual const char* what() const noexcept {
		return msg.c_str();
    }
};

