// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks
//
// A check is a `static void` function with no arguments. Each file registers its checks
// in one table, which the runner in `tests/check.cpp` runs in order:
//
//   static check::Suite
//   check_by_name {
//       {"sieve matches trial division", sieve_matches_trial_division},
//   };
//
// Check objects compile with `-Werror=unused-function`, so a check missing from its
// table fails the build.
//
// A table belongs to a tier. `bin/check` runs the default tier, which `make check` runs; a
// table registered for the engine tier, whose checks take minutes rather than seconds, runs only
// under `bin/check engine`, which `make check-engine` runs after the default tier:
//
//   static check::Suite
//   engine_check_by_name {check::Tier::ENGINE, {
//       {"squarings equal BigInt's at every supported length", squarings_at_every_length},
//   }};
//

#pragma once


// Standard libs
#include <initializer_list>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


namespace check {


	using Function = void (*)();


	// Which run of bin/check a case belongs to: the default, or `bin/check engine`
	enum class Tier {
		DEFAULT,
		ENGINE,
	};


	struct Case {
		std::string label;
		Function function;
		std::string where = "";        // the table's file:line, set on registration
		Tier tier = Tier::DEFAULT;     // the table's tier, set on registration
	};


	class failure: public std::runtime_error {
	public:
		failure(const std::string& what): std::runtime_error(what) {}

	};


	std::vector<Case>& registry();


	class Suite {
	public:
		Suite(std::initializer_list<Case> cases,
		      std::source_location location = std::source_location::current());
		Suite(Tier tier, std::initializer_list<Case> cases,
		      std::source_location location = std::source_location::current());

	};


	std::string where(const std::source_location& location);


	void record(bool passed, const std::string& what, const std::source_location& location,
	            const std::string& detail = "");


	void expect(bool condition, const std::string& what,
	            std::source_location location = std::source_location::current());


	void require(bool condition, const std::string& what,
	             std::source_location location = std::source_location::current());


	template <typename Left, typename Right>
	void expect_equal(const Left& left, const Right& right, const std::string& what,
	                  std::source_location location = std::source_location::current()) {
		//
		// Expect two values to compare equal
		//
		// On failure both values are printed beside the expectation.

		if (left == right) {
			record(true, what, location);
			return;
		}
		std::ostringstream detail;
		detail << std::boolalpha << "left: " << left << ", right: " << right;
		record(false, what, location, detail.str());
	}

}
