// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// String methods
//


// Standard libs
#include <map>
#include <regex>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>    // getenv
#include <unistd.h>   // isatty, fileno

// Internal libs
#include "ansi.hpp"


static bool NO_COLOR = false;
static bool FORCE_COLOR = false;
static bool STDOUT_COLOR = true;
static bool STDERR_COLOR = true;


void init_tty() {
	// Check environment variables and I/O context to decide if we should
	// be printing characters with ansi color sequences or suppress them.
	//
	// NOTE: This should be called by main() once at startup.

	if (not isatty(fileno(stdout))) {
		STDOUT_COLOR = false;
	}

	if (not isatty(fileno(stderr))) {
		STDERR_COLOR = false;
	}

	if (std::getenv("NO_COLOR")) {
		NO_COLOR = true;
		STDOUT_COLOR = false;
		STDERR_COLOR = false;
	}

	if (std::getenv("FORCE_COLOR")) {
		FORCE_COLOR = true;
		STDOUT_COLOR = true;
		STDERR_COLOR = true;
	}
}


std::string format_ansi(Ansi ansi, const std::string& text) {
	// Wrap `text` with given `ansi` escape sequence and RESET (if necessary).
	//
	// This method considers NO_COLOR or FORCE_COLOR to implicitly
	// suppress applying ansi color escape sequences.
	//
	// TODO: We need a better way to deal with <stdout> vs <stderr>.
	//       Right now `log` methods are only used here so we'll assume
	//       only <stderr> for now.

    if (NO_COLOR and not FORCE_COLOR) {
        return text;
	}
	if (not STDERR_COLOR) {
		return text;
	}
	if (text.ends_with(ansi_by_id[RESET])) {
		return ansi_by_id[ansi] + text;
	}
	return ansi_by_id[ansi] + text + ansi_by_id[RESET];
}


std::string bold(const std::string& text) {
	return format_ansi(BOLD, text);
}

std::string faint(const std::string& text) {
	return format_ansi(FAINT, text);
}

std::string italic(const std::string& text) {
	return format_ansi(ITALIC, text);
}

std::string underline(const std::string& text) {
	return format_ansi(UNDERLINE, text);
}

std::string black(const std::string& text) {
	return format_ansi(BLACK, text);
}

std::string red(const std::string& text) {
	return format_ansi(RED, text);
}

std::string green(const std::string& text) {
	return format_ansi(GREEN, text);
}

std::string yellow(const std::string& text) {
	return format_ansi(YELLOW, text);
}

std::string blue(const std::string& text) {
	return format_ansi(BLUE, text);
}

std::string magenta(const std::string& text) {
	return format_ansi(MAGENTA, text);
}

std::string cyan(const std::string& text) {
	return format_ansi(CYAN, text);
}

std::string white(const std::string& text) {
	return format_ansi(WHITE, text);
}



std::string colorize_usage(const std::string& text) {

	std::string output = text;

	// A header starts a line and may hold a space ("Exit status:"); no other line starts
	// with a letter
	std::regex header_re("^[A-Za-z][A-Za-z ]*:", std::regex::multiline);
	output = std::regex_replace(output, header_re, bold("$&"));

	std::regex metavar_re("(PVAL|NAME|PATH)", std::regex::multiline);
	output = std::regex_replace(output, metavar_re, italic("$&"));

	// An option starts the text or follows a space, '[' or '(', so the hyphens inside a
	// quoted name such as 'p-minus-1' are left alone
	std::regex option_re("(^|[ \\[(])(-[a-zA-Z]+|--[a-z]+(-[a-z]+)?)", std::regex::multiline);
	output = std::regex_replace(output, option_re, "$1" + cyan("$2"));

	std::regex quoted_re("'(.*?)'", std::regex::multiline);
	output = std::regex_replace(output, quoted_re, yellow("$&"));

	return output;
}

