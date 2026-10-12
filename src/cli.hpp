// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Command-line Interface
//

#pragma once


// Standard libs
#include <iostream>
#include <string>
#include <map>

// Internal libs
#include "main.hpp"
#include "exc.hpp"
#include "io.hpp"


namespace cli {


	class show_info: public mps_error {
	public:
		show_info(const std::string& msg = ""): mps_error(msg) {}

	};


	class argument_error: public mps_error {
	public:
		argument_error(const std::string& msg = ""): mps_error(msg) {}

	};


	enum TestCase {
		TEST_NONE,
		TEST_ALL,
		TEST_FACTOR,
		TEST_P_MINUS_ONE,
		TEST_LUCAS_LEHMER,
	};


	static std::map<std::string, TestCase>
	test_by_name {
		{"none",         TEST_NONE},
		{"all",          TEST_ALL},
		{"qf",           TEST_FACTOR},
		{"quick",        TEST_FACTOR},
		{"factor",       TEST_FACTOR},
		{"pm",           TEST_P_MINUS_ONE},
		{"p-min",        TEST_P_MINUS_ONE},
		{"ll",           TEST_LUCAS_LEHMER},
		{"lucas-lehmer", TEST_LUCAS_LEHMER},
	};

	class Interface {
	public:

		Interface(const int argc, const char** argv);
		~Interface();

		std::string pval;

		TestCase test_case = TEST_ALL;
		bool opt_test_case = false;
		void set_test_case(const std::string& str);

		std::string file_path = "-";
		bool opt_file_path = false;
		void set_file_path(const std::string& str);

		bool show_primes = false;
		bool skip_ptest = false;

		bool debug_mode = false;
		bool detailed_mode = false;

		std::vector<std::string> remaining {};
		std::vector<std::string> positional {};
		std::string opt;
		std::string opt_value;
		bool positional_only = false; // flagged after '--'

		void parse_args();
		void check_option_long();
		void check_option_short();
		void pop_opt();
	};

	Interface parse_args(const int argc, const char** argv);
}

