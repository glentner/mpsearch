// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Command-line Interface
//


// Standard libs
#include <charconv>
#include <iostream>
#include <string>
#include <map>
#include <system_error>

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

			if (positional_only) {
				positional.push_back(opt);

			} else if (opt == "--") {
				positional_only = true;

			} else if (starts_with(opt, "--")) {
				check_option_long();

			} else if (opt.size() > 1 and starts_with(opt, "-")) {
				check_option_short();

			} else {
				positional.push_back(opt);
			}
		}

		if (positional.size() < 1)
			throw argument_error("Missing positional argument: PVAL");

		if (positional.size() > 1)
			throw argument_error("Unexpected positional argument: " + positional[1]);

		set_pval(positional[0]);

		if (not opt_file_path) {
			file_path = pval + ".mps";
		}
	}

	void Interface::pop_opt() {
		opt = remaining[0];
		remaining.erase(remaining.begin());
	}

	void Interface::check_option_long() {
		//
		// Read one long option, as --name or --name=VALUE
		//
		// The word splits at its first '=', so a value may hold one too. A valued option takes
		// the text after it, even when that is empty, or else the next word; a flag given a value
		// is an error.

		const size_t split = opt.find('=');
		const std::string name = opt.substr(0, split);
		const bool has_value = split != std::string::npos;

		const auto found = flag_by_option.find(name);
		if (found == flag_by_option.end())
			throw argument_error("Unrecognized option: " + name);

		const char flag = found->second;
		if (flag == 't' or flag == 'f') {
			set_value(flag, has_value ? opt.substr(split + 1) : pop_value(flag));
		} else if (has_value) {
			throw argument_error("Option " + name + " takes no value");
		} else {
			check_flag(flag);
		}
	}

	void Interface::check_option_short() {
		//
		// Read one word of short options, one character at a time
		//
		// Flags may be grouped, and -dD is -d -D. A valued option takes the rest of the word,
		// less one leading '=', or the next word when nothing follows it, and ends the word.
		// The help and the version end the run where they stand, so -hx is the help.

		const std::string word = opt;  // pop_opt overwrites opt
		for (size_t i = 1; i < word.size(); i++) {
			const char flag = word[i];
			if (flag == 't' or flag == 'f') {
				const std::string rest = word.substr(i + 1);
				set_value(flag, rest.empty() ? pop_value(flag) : strip_left(rest, "="));
				return;
			}
			check_flag(flag);
		}
	}

	void Interface::check_flag(const char flag) {
		//
		// Apply one flag, given short or long

		switch (flag) {

			case 'h':
				throw show_info(APP_HELP);

			case 'v':
				throw show_info(APP_NAME + " v" + APP_VERSION);

			case 'p':
				if (show_primes)
					logging::warn("Duplicate use of -p/--primes");
				show_primes = true;
				break;

			case 's':
				if (skip_ptest)
					logging::warn("Duplicate use of -s/--skip-ptest");
				skip_ptest = true;
				break;

			case 'j':
				if (json_mode)
					logging::warn("Duplicate use of -j/--json");
				json_mode = true;
				break;

			case 'd':
				if (debug_mode)
					logging::warn("Duplicate use of -d/--debug");
				debug_mode = true;
				break;

			case 'D':
				if (detailed_mode)
					logging::warn("Duplicate use of -D/--detailed");
				detailed_mode = true;
				break;

			default:
				throw argument_error(std::string("Unrecognized option: -") + flag);
		}
	}

	void Interface::set_value(const char flag, const std::string& value) {
		//
		// Give a valued option its value, however it was spelled

		if (flag == 't') {
			if (opt_test_case)
				logging::warn("Duplicate use of -t/--test");
			set_test_case(value);

		} else {
			if (opt_file_path)
				logging::warn("Duplicate use of -f/--file");
			set_file_path(value);
		}
	}

	std::string Interface::pop_value(const char flag) {
		//
		// Take the next word as a valued option's value

		if (remaining.empty())
			throw argument_error(flag == 't' ? "Missing value for -t/--test" : "Missing value for -f/--file");
		pop_opt();
		return opt;
	}

	void Interface::set_pval(const std::string& str) {
		//
		// Take PVAL as typed and validate its value
		//
		// PVAL is one or more ASCII digits, leading zeros allowed, whose value is from 2 to
		// PVAL_MAX, or from 0 with -p/--primes. `std::from_chars` takes no sign and no space,
		// reads only ASCII digits and reports overflow; anything it leaves unread is junk.
		// Called once the options are read, so -p counts wherever it is given.

		pval = str;
		const long unsigned min = show_primes ? 0 : 2;
		long unsigned value = 0;
		const char* last = str.data() + str.size();
		const auto [ptr, ec] = std::from_chars(str.data(), last, value);
		if (ec != std::errc() or ptr != last or value < min or value > PVAL_MAX)
			throw argument_error("Invalid PVAL '" + str + "': expected a decimal integer from " +
			                     std::to_string(min) + " to " + std::to_string(PVAL_MAX) +
			                     (show_primes ? " with -p/--primes" : ""));
		pval_number = value;
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
		//
		// Record the checkpoint path as given
		//
		// Nothing opens or creates the file here. Checkpoints are not written yet, and reading
		// the options should leave nothing on disk.

		if (str.empty())
			throw argument_error("Empty value for -f/--file");
		opt_file_path = true;
		file_path = str;
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

