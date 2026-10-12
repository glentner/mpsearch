// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Command-line Interface
//


// Standard libs
#include <iostream>
#include <fstream>
#include <string>
#include <map>

// Internal libs
#include "main.hpp"
#include "log.hpp"
#include "cli.hpp"


// Helper methods for processing command-line options
bool starts_with(const std::string& opt, const std::string& match_str);
std::string strip_left(const std::string opt, const std::string& match_str);


namespace cli {

	Interface parse_args(const int argc, const char** argv) {
		Interface opt(argc, argv);
		opt.parse_args();
		return opt;
	}

	Interface::Interface(const int argc, const char** argv) {
		for (int i = 0; i < argc; i++) {
			remaining.push_back(argv[i]);
		}
	}

	Interface::~Interface() {
	}

	void Interface::parse_args() {

		pop_opt();  // arg[0] is path to executable

		if (remaining.empty())
			throw show_info(APP_USAGE);

		while (not remaining.empty()) {

			pop_opt();

			if (opt == "--" or positional_only) {
				positional.push_back(opt);
				positional_only = true;
				continue;

			} else if (starts_with(opt, "--")) {
				check_option_long();

			} else if (starts_with(opt, "-")) {
				check_option_short();

			} else {
				positional.push_back(opt);
			}
		}

		if (positional.size() < 1)
			throw argument_error("Missing positional argument: PVAL");

		if (positional.size() > 1)
			throw argument_error("Unexpected positional argument: " + positional[1]);

		pval = positional[0];

		if (pval.size() > 9) {
			throw argument_error("PVAL must be less than ~2^30 (9 digits)");
		}

		if (not opt_file_path) {
			file_path = pval + ".mps";
		}
	}

	void Interface::pop_opt() {
		opt = remaining[0];
		remaining.erase(remaining.begin());
	}

	void Interface::check_option_long() {

		if (opt == "--help") {
			throw show_info(APP_HELP);

		} else if (opt == "--version") {
			throw show_info(APP_NAME + " v" + APP_VERSION);

		} else if (opt == "--test") {
			if (remaining.empty())
				throw argument_error("Missing option value for --test");
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			set_test_case(remaining[0]);
			pop_opt();

		} else if (starts_with(opt, "--test=")) {
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			opt_value = strip_left(opt, "--test=");
			set_test_case(opt_value);

		} else if (opt == "--file") {
			if (remaining.empty())
				throw argument_error("Missing option value for --file");
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			set_test_case(remaining[0]);
			pop_opt();

		} else if (starts_with(opt, "--file=")) {
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			opt_value = strip_left(opt, "--file=");
			set_file_path(opt_value);

		} else if (opt == "--skip-ptest") {
			if (skip_ptest)
				logging::warn("Duplicate use of -s/--skip-ptest");
			skip_ptest = true;

		} else if (opt == "--primes") {
			if (show_primes)
				logging::warn("Duplicate use of -p/--primes");
			show_primes = true;

		} else if (opt == "--debug") {
			if (debug_mode)
				logging::warn("Duplicate use of -d/--debug");
			debug_mode = true;

		} else if (opt == "--detailed") {
			if (detailed_mode)
				logging::warn("Duplicate use of -D/--detailed");
			detailed_mode = true;

		} else {
			throw argument_error("Unrecognized option: " + opt);
		}
	}

	void Interface::check_option_short() {

		if (opt == "-h" or starts_with(opt, "-h")) {
			throw show_info(APP_HELP);

		} else if (opt == "-v" or starts_with(opt, "-v")) {
			throw show_info(APP_NAME + " v" + APP_VERSION);

		} else if (opt == "-t") {
			if (remaining.empty())
				throw argument_error("Missing option value for -t/--test");
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			set_test_case(remaining[0]);
			pop_opt();

		} else if (starts_with(opt, "-t")) {
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			opt_value = strip_left(opt, "-t");
			set_test_case(opt_value);

		} else if (starts_with(opt, "-t=")) {
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			opt_value = strip_left(opt, "-t=");
			set_test_case(opt_value);

		} else if (opt == "-f") {
			if (remaining.empty())
				throw argument_error("Missing option value for -f/--file");
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			set_file_path(remaining[0]);
			pop_opt();

		} else if (starts_with(opt, "-f")) {
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			opt_value = strip_left(opt, "-f");
			set_file_path(opt_value);

		} else if (starts_with(opt, "-f=")) {
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			opt_value = strip_left(opt, "-f=");
			set_file_path(opt_value);

		} else if (opt == "-s") {
			if (skip_ptest)
				logging::warn("Duplicate use of -s/--skip-ptest");
			skip_ptest = true;

		} else if (opt == "-p") {
			if (show_primes)
				logging::warn("Duplicate use of -p/--primes");
			show_primes = true;

		} else if (opt == "-d") {
			if (debug_mode)
				logging::warn("Duplicate use of -d/--debug");
			debug_mode = true;

		} else if (opt == "-D") {
			if (detailed_mode)
				logging::warn("Duplicate use of -D/--detailed");
			detailed_mode = true;

		} else {
			throw argument_error("Unrecognized option: " + opt);
		}

	}

	void Interface::set_test_case(const std::string& str) {
		opt_test_case = true;
		if (test_by_name.find(str) != test_by_name.end()) {
			test_case = test_by_name[str];
		} else {
			throw argument_error("Unrecognized option value '" + str + "' for -t/--test");
		}
	}

	void Interface::set_file_path(const std::string& str) {
		opt_file_path = true;
		file_path = str;
		std::ofstream file(file_path, std::ofstream::app);
		if (file.fail()) {
			throw argument_error("Could not open file: " + file_path);
		} else {
			file.close();
		}
	}

}


bool starts_with(const std::string& opt, const std::string& match_str) {
	if (opt.size() < match_str.size()) return false;
	return opt.substr(0, match_str.size()) == match_str;
}

std::string strip_left(const std::string opt, const std::string& match_str) {
	if (starts_with(opt, match_str)) {
		return opt.substr(match_str.size(), opt.size());
	}
	return opt;
}

