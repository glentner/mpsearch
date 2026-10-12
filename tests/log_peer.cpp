// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: a second unit for the logger
//
// This file sorts last among the checks, so its one check registers, and runs, last: it
// finds the logger's state as the run began, or a check before it failed to restore it.
//


// Standard libs
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

// Internal libs
#include "check.hpp"
#include "log.hpp"
#include "log_peer.hpp"


// Messages go through a type local to this file, so the linker never merges this file's
// instances of the logger's templates with another unit's, and what is logged here reads
// the state through this file's code.
namespace {


	struct Message {
		std::string text;
	};


	std::ostream& operator<<(std::ostream& s, const Message& message) {
		return s << message.text;
	}

}


namespace log_peer {


	Addresses addresses() {
		//
		// Where the logger's state lies, as this unit sees it

		return {&logging::style, &logging::level, &logging::hostname, &logging::task_id, &logging::name};
	}


	void set_level(logging::Level level) {
		logging::level = level;
	}


	void set_style(logging::Style style) {
		logging::style = style;
	}


	std::string debug(const std::string& text) {
		//
		// Log `text` at debug from this unit
		//
		// Returns what reached `std::cerr`, escape sequences removed.

		CaptureCerr capture;
		logging::debug(Message {text});
		return capture.text();
	}


	std::string info(const std::string& text) {
		//
		// Log `text` at info from this unit
		//
		// Returns what reached `std::cerr`, escape sequences removed.

		CaptureCerr capture;
		logging::info(Message {text});
		return capture.text();
	}


	std::string plain(const std::string& text) {
		//
		// Remove the escape sequences that colour a line
		//
		// `bin/check` never calls `init_tty()`, so what it logs is always coloured.

		std::string out;
		size_t i = 0;
		while (i < text.size()) {
			if (text[i] == '\033' and i + 1 < text.size() and text[i + 1] == '[') {
				size_t end = i + 2;
				while (end < text.size() and (std::isdigit(static_cast<unsigned char>(text[end])) or text[end] == ';'))
					end++;
				if (end < text.size() and text[end] == 'm') {
					i = end + 1;
					continue;
				}
			}
			out += text[i];
			i++;
		}
		return out;
	}


	bool stamped(const std::string& line, const std::string& rest) {
		//
		// Whether `line` is a date, a space, a time, a space, then exactly `rest`
		//
		// The date is `YYYY-MM-DD` and the time `HH:MM:SS`, matched by their shape, never by
		// their value.

		const std::string shape = "dddd-dd-dd dd:dd:dd ";
		if (line.size() != shape.size() + rest.size())
			return false;
		for (size_t i = 0; i < shape.size(); i++) {
			const bool digit = std::isdigit(static_cast<unsigned char>(line[i]));
			if (shape[i] == 'd' ? not digit : line[i] != shape[i])
				return false;
		}
		return line.compare(shape.size(), std::string::npos, rest) == 0;
	}


	SavedLogger::SavedLogger():
		style(logging::style),
		level(logging::level),
		hostname(logging::hostname),
		task_id(logging::task_id),
		name(logging::name) {}


	SavedLogger::~SavedLogger() {
		logging::style = style;
		logging::level = level;
		logging::hostname = hostname;
		logging::task_id = task_id;
		logging::name = name;
	}


	CaptureCerr::CaptureCerr():
		saved(std::cerr.rdbuf(buffer.rdbuf())) {}


	CaptureCerr::~CaptureCerr() {
		std::cerr.rdbuf(saved);
	}


	std::string CaptureCerr::text() const {
		return plain(buffer.str());
	}

}


static void the_state_is_as_the_run_began() {
	//
	// No check leaves the logger's state changed
	//
	// `bin/check` never calls `logging::init()`, so the state is as zero-initialisation
	// leaves it: the default style, the debug level and three empty strings.

	check::expect(logging::style == logging::DEFAULT, "the style is DEFAULT");
	check::expect(logging::level == logging::DEBUG, "the level is DEBUG");
	check::expect(logging::hostname.empty(), "the host name is empty");
	check::expect(logging::task_id.empty(), "the task id is empty");
	check::expect(logging::name.empty(), "the name is empty");
}


static check::Suite
check_by_name {
	{"the logger's state is as the run began", the_state_is_as_the_run_began},
};
