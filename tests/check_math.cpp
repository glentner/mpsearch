// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: mathematical operations
//
// The sieve is checked only for n >= 3 and from index 2 up: its entry for 1 and its
// behaviour below 3 are known to be wrong (see `issues/`).
//


// Standard libs
#include <cstddef>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "math.hpp"


static bool is_prime_by_trial_division(size_t n) {

	if (n < 2)
		return false;
	for (size_t d = 2; d * d <= n; d++)
		if (n % d == 0)
			return false;
	return true;
}


static void sieve_matches_trial_division() {
	//
	// The sieve agrees with trial division
	//
	// For every i in [2, n), at bounds on both sides of a square and a prime.

	for (size_t n: {3, 4, 5, 9, 10, 26, 30, 31, 97, 100, 1'000, 10'000}) {
		std::vector<bool> is_prime = prime_sieve(n);
		check::require(is_prime.size() == n, "prime_sieve(" + std::to_string(n) + ") has n entries");

		size_t mismatches = 0;
		for (size_t i = 2; i < n; i++)
			if (is_prime[i] != is_prime_by_trial_division(i))
				mismatches++;
		check::expect_equal(mismatches, size_t(0), "prime_sieve(" + std::to_string(n) + ") matches trial division");
	}
}


static void sieve_marks_squares_at_the_bound() {
	//
	// A square just below the bound is composite
	//
	// With n = p * p + 1, the last index is p * p, which the sieve must still reach.

	for (size_t p: {3, 5, 7, 11, 13, 97}) {
		size_t n = p * p + 1;
		std::vector<bool> is_prime = prime_sieve(n);
		check::require(is_prime.size() == n, "prime_sieve(" + std::to_string(n) + ") has n entries");
		check::expect(not is_prime[p * p], std::to_string(p * p) + " marked composite in prime_sieve(" + std::to_string(n) + ")");
	}
}


static check::Suite
check_by_name {
	{"sieve matches trial division",     sieve_matches_trial_division},
	{"sieve marks squares at the bound", sieve_marks_squares_at_the_bound},
};
