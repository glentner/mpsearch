// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: logging
//
// `<cmath>` comes before the logger on purpose: the C library's `log` and the logger's
// namespace must compile together in one translation unit.
//
// The logger holds one state for the whole program. The checks set it here and log from
// `log_peer.cpp`, a second unit, or the other way round, so a state that is not shared
// fails them. Each check sets the state by assignment, with the fake host `check-host`,
// captures what it logs, and restores the state before it returns.
//


// Standard libs
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

// Internal libs
#include "check.hpp"
#include "log.hpp"
#include "log_peer.hpp"


// Messages go through a type local to this file, so the linker never merges this file's
// instances of the logger's templates with another unit's, and a check reads the state
// through this file's code.
namespace {


	struct Message {
		std::string text;
	};


	std::ostream& operator<<(std::ostream& s, const Message& message) {
		return s << message.text;
	}

}


static std::string logged_debug(const std::string& text) {
	//
	// Log `text` at debug from this unit
	//
	// Returns what reached `std::cerr`, escape sequences removed.

	log_peer::CaptureCerr capture;
	logging::debug(Message {text});
	return capture.text();
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

	log_peer::SavedLogger saved;
	logging::level = logging::WARNING;
	logging::style = logging::DEFAULT;

	std::string below, at, above;
	{
		log_peer::CaptureCerr capture;
		logging::info(Message {"below the level"});
		below = capture.text();
	}
	{
		log_peer::CaptureCerr capture;
		logging::warning(Message {"at the level"});
		at = capture.text();
	}
	{
		log_peer::CaptureCerr capture;
		logging::critical(Message {"above the level"});
		above = capture.text();
	}

	check::expect_equal(below, std::string(""), "info is filtered out at WARNING");
	check::expect(at.find("at the level") != std::string::npos, "warning is printed at WARNING");
	check::expect(above.find("above the level") != std::string::npos, "critical is printed at WARNING");
}


static void the_state_is_one_copy() {
	//
	// The style, the level, the host name, the task id and the name lie at one address
	//
	// Seen from this unit and from `log_peer.cpp` alike.

	const log_peer::Addresses peer = log_peer::addresses();
	check::expect(peer.style == static_cast<const void*>(&logging::style), "the style is the peer's");
	check::expect(peer.level == static_cast<const void*>(&logging::level), "the level is the peer's");
	check::expect(peer.hostname == static_cast<const void*>(&logging::hostname), "the host name is the peer's");
	check::expect(peer.task_id == static_cast<const void*>(&logging::task_id), "the task id is the peer's");
	check::expect(peer.name == static_cast<const void*>(&logging::name), "the name is the peer's");
}


static void the_level_holds_across_units() {
	//
	// A level set in one unit filters the debug messages another logs
	//
	// Set here, it holds for the peer; set in the peer, it holds here.

	log_peer::SavedLogger saved;
	logging::style = logging::DEFAULT;

	logging::level = logging::INFO;
	check::expect_equal(log_peer::debug("from the peer at info"), std::string(""),
	                    "the peer's debug is filtered out at INFO set here");
	logging::level = logging::DEBUG;
	check::expect_equal(log_peer::debug("from the peer at debug"), std::string("   debug [mps] from the peer at debug\n"),
	                    "the peer's debug is printed at DEBUG set here");

	log_peer::set_level(logging::INFO);
	check::expect_equal(logged_debug("from here at info"), std::string(""),
	                    "this unit's debug is filtered out at INFO set in the peer");
	log_peer::set_level(logging::DEBUG);
	check::expect_equal(logged_debug("from here at debug"), std::string("   debug [mps] from here at debug\n"),
	                    "this unit's debug is printed at DEBUG set in the peer");
}


static void the_detailed_style_holds_across_units() {
	//
	// The style, the host name and the task id set here stamp the peer's messages
	//
	// The detailed style stamps the time, the host and the task id; with no task id there
	// is no task stamp; the default style has no stamp at all.

	log_peer::SavedLogger saved;
	logging::level = logging::INFO;
	logging::style = logging::DETAILED;
	logging::hostname = "check-host";
	logging::task_id = "abc";

	check::expect(log_peer::stamped(log_peer::info("from the peer"), "check-host [abc]     info [mps] from the peer\n"),
	              "the peer's info line is stamped with the host and the task id set here");

	logging::task_id = "";
	check::expect(log_peer::stamped(log_peer::info("from the peer"), "check-host     info [mps] from the peer\n"),
	              "the peer's info line has no task stamp with the task id empty here");

	logging::task_id = "abc";
	logging::style = logging::DEFAULT;
	check::expect_equal(log_peer::info("from the peer"), std::string("    info [mps] from the peer\n"),
	                    "the peer's info line has no stamp in the default style set here");
}


static check::Suite
check_by_name {
	{"cmath and the logger together",          cmath_and_logger_together},
	{"level filter ordering",                  level_filter_ordering},
	{"the logger's state is one copy",         the_state_is_one_copy},
	{"the level holds across units",           the_level_holds_across_units},
	{"the detailed style holds across units",  the_detailed_style_holds_across_units},
};
