// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: logging
//
// `<cmath>` comes before the logger on purpose: the C library's `log` and the logger's
// namespace must compile together in one translation unit.
//


// Standard libs
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

// Internal libs
#include "check.hpp"
#include "log.hpp"


// The logger's state is `static`, one copy per translation unit. Its templates
// instantiated with a type local to this file read this file's copy, never another
// module's, and the level is set directly for the same reason.
namespace {


	struct Message {
		std::string text;
	};


	std::ostream& operator<<(std::ostream& s, const Message& message) {
		return s << message.text;
	}

}


static void cmath_and_logger_together() {
	//
	// The C library's log and the logger in one unit

	check::expect_equal(std::log(1.0), 0.0, "std::log(1.0) is 0");
	check::expect_equal(logging::level_by_name["info"], logging::INFO, "level_by_name[\"info\"] is INFO");
	check::expect_equal(logging::name_by_level[logging::CRITICAL], std::string("critical"),
	                    "name_by_level[CRITICAL] is \"critical\"");
}


static void level_filter_ordering() {
	//
	// Levels order from debug to critical
	//
	// A message is printed only at or above the current level.

	check::expect(logging::DEBUG < logging::INFO and
	              logging::INFO < logging::WARNING and
	              logging::WARNING < logging::ERROR and
	              logging::ERROR < logging::CRITICAL, "DEBUG < INFO < WARNING < ERROR < CRITICAL");

	logging::Level saved_level = logging::level;
	logging::Style saved_style = logging::style;
	logging::level = logging::WARNING;
	logging::style = logging::DEFAULT;

	std::ostringstream below, at, above;
	std::streambuf* saved = std::cerr.rdbuf(below.rdbuf());
	logging::info(Message {"below the level"});
	std::cerr.rdbuf(at.rdbuf());
	logging::warning(Message {"at the level"});
	std::cerr.rdbuf(above.rdbuf());
	logging::critical(Message {"above the level"});
	std::cerr.rdbuf(saved);

	logging::level = saved_level;
	logging::style = saved_style;

	check::expect_equal(below.str(), std::string(""), "info is filtered out at WARNING");
	check::expect(at.str().find("at the level") != std::string::npos, "warning is printed at WARNING");
	check::expect(above.str().find("above the level") != std::string::npos, "critical is printed at WARNING");
}


static check::Suite
check_by_name {
	{"cmath and the logger together", cmath_and_logger_together},
	{"level filter ordering",         level_filter_ordering},
};
