// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Remediation probe (review cycle 1, F3): the exponent check reports a composite only through its
// exact re-check. Drives the tree's check_exponent_by with factor searches that lie, and
// check_exponent with the true one; prints each failure and exits non-zero on any.
//


// Standard libs
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// Internal libs
#include "math.hpp"
#include "result.hpp"


static long unsigned says_zero(const long unsigned) { return 0; }
static long unsigned says_one(const long unsigned) { return 1; }
static long unsigned says_five(const long unsigned) { return 5; }
static long unsigned says_eleven(const long unsigned) { return 11; }

static int failures = 0;


static void want_error(const char* what, const std::optional<Result>& got) {
	if (not got or got->outcome() != OUTCOME_ERROR) {
		std::printf("FAIL %s: want an error, never a composite\n", what);
		failures++;
	}
}


static void want_factor(const char* what, const std::optional<Result>& got, const std::string& factor) {
	if (not got or got->outcome() != OUTCOME_COMPOSITE or got->factors() != std::vector<std::string>{factor}) {
		std::printf("FAIL %s: want a composite by the factor %s\n", what, factor.c_str());
		failures++;
	}
}


static void want_none(const char* what, const std::optional<Result>& got) {
	if (got) {
		std::printf("FAIL %s: want no result for a prime exponent\n", what);
		failures++;
	}
}


int main() {
	want_error("check_exponent_by(33, a search that says 0)", check_exponent_by(33, says_zero));
	want_error("check_exponent_by(33, a search that says 1)", check_exponent_by(33, says_one));
	want_error("check_exponent_by(33, a search that says 5)", check_exponent_by(33, says_five));
	want_factor("check_exponent_by(33, a search that says 11)", check_exponent_by(33, says_eleven), "2^11-1");
	want_factor("check_exponent_by(33, smallest_prime_factor)", check_exponent_by(33, smallest_prime_factor), "2^3-1");
	want_none("check_exponent_by(31, smallest_prime_factor)", check_exponent_by(31, smallest_prime_factor));
	want_factor("check_exponent(33)", check_exponent(33), "2^3-1");
	want_factor("check_exponent(999002449)", check_exponent(999002449), "2^31607-1");
	want_none("check_exponent(31)", check_exponent(31));
	want_none("check_exponent(999999937)", check_exponent(999999937));
	return failures == 0 ? 0 : 1;
}
