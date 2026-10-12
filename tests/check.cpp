// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Check runner
//
// Runs every registered check in order and reports through plain iostreams, never the
// logger, which is itself under check. Before each body it prints `check <label>`; each
// failure prints ` FAIL <label>` at once and again in the summary, whose last line is
// `check: <passed> passed, <failed> failed`. The exit status is 1 on any failure.
//


// Standard libs
#include <cstdlib>     // atexit, _Exit
#include <exception>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"


static int expectations = 0;
static int failures = 0;

static bool running = false;
static std::string current_label;


namespace check {


	std::vector<Case>& registry() {
		//
		// All registered checks
		//
		// A function-local static, so a table in any file may register itself during
		// static initialization, whatever the order.

		static std::vector<Case> cases;
		return cases;
	}


	Suite::Suite(std::initializer_list<Case> cases, std::source_location location) {
		for (const Case& entry: cases)
			registry().push_back({entry.label, entry.function, where(location)});
	}


	std::string where(const std::source_location& location) {
		return std::string(location.file_name()) + ":" + std::to_string(location.line());
	}


	void record(bool passed, const std::string& what, const std::source_location& location,
	            const std::string& detail) {
		//
		// Count one expectation
		//
		// A failed expectation prints where it was made and what it expected.

		expectations++;
		if (passed)
			return;

		failures++;
		std::cout << "   " << where(location) << ": expected " << what;
		if (not detail.empty())
			std::cout << " (" << detail << ")";
		std::cout << std::endl;
	}


	void expect(bool condition, const std::string& what, std::source_location location) {
		record(condition, what, location);
	}


	void require(bool condition, const std::string& what, std::source_location location) {
		//
		// Expect a condition, ending the check if it fails

		record(condition, what, location);
		if (not condition)
			throw failure(what);
	}

}


static void exit_guard() {
	//
	// Fail a run that a check ends early
	//
	// Registered with `atexit`, so a check that calls `exit`, even `exit(0)`, still fails
	// the run instead of passing it with the remaining checks unrun.

	if (not running)
		return;

	std::cout << "   exit called during the check" << std::endl;
	std::cout << " FAIL " << current_label << std::endl;
	std::_Exit(1);
}


static std::string run_case(const check::Case& entry) {
	//
	// Run one check
	//
	// Returns why the check failed, or an empty string if it passed.

	expectations = 0;
	failures = 0;

	try {
		entry.function();

	} catch (check::failure const&) {
		// Already recorded by `require`

	} catch (std::exception const& error) {
		return std::string("uncaught exception: ") + error.what();

	} catch (...) {
		return "uncaught exception of unknown type";
	}

	if (failures > 0)
		return std::to_string(failures) + " of " + std::to_string(expectations) + " expectations failed";

	if (expectations == 0)
		return "no expectations ran";

	return "";
}


int main() {

	std::atexit(exit_guard);

	const std::vector<check::Case>& cases = check::registry();
	if (cases.empty()) {
		std::cout << " FAIL (no checks registered)" << std::endl;
		std::cout << "check: 0 passed, 1 failed" << std::endl;
		return 1;
	}

	std::map<std::string, int> count_by_label;
	for (const check::Case& entry: cases)
		count_by_label[entry.label]++;

	int passed = 0;
	std::vector<std::string> failed;

	running = true;
	for (const check::Case& entry: cases) {

		current_label = entry.label;
		std::cout << "check " << entry.label << std::endl;

		std::string reason;
		if (count_by_label[entry.label] > 1)
			reason = "label registered " + std::to_string(count_by_label[entry.label]) +
			         " times (" + entry.where + ")";
		else
			reason = run_case(entry);

		if (reason.empty()) {
			passed++;
		} else {
			std::cout << "   " << reason << std::endl;
			std::cout << " FAIL " << entry.label << std::endl;
			failed.push_back(entry.label);
		}
	}
	running = false;

	std::cout << std::endl;
	for (const std::string& label: failed)
		std::cout << " FAIL " << label << std::endl;
	std::cout << "check: " << passed << " passed, " << failed.size() << " failed" << std::endl;

	return failed.empty() ? 0 : 1;
}
