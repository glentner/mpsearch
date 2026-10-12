// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: mathematical operations
//
// The sieve is checked entry by entry for every n below four and from index 0 up for
// larger n. A write past the last entry for n = 1 or 2 lands inside the vector's own
// storage word, where no value check can see it; the standard libraries' bounds
// assertions can. `make check` builds the checks with them, so a write outside the
// sieve's entries stops bin/check. libc++ checks from its fast hardening mode up, and
// libstdc++ checks vector<bool> from GCC 15; an older libstdc++ sees the values only.
//


// Standard libs
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
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


static void sieve_below_four_exactly() {
	//
	// The sieve for n below four, entry by entry
	//
	// Nothing below 2 is prime, so n < 3 gives n false entries, and n = 3 marks only 2.

	static const std::vector<std::vector<bool>> expected = {
		{},
		{false},
		{false, false},
		{false, false, true},
	};

	for (size_t n = 0; n < expected.size(); n++) {
		std::vector<bool> is_prime = prime_sieve(n);
		check::expect_equal(is_prime.size(), n, "prime_sieve(" + std::to_string(n) + ") has n entries");
		if (is_prime.size() != n)
			continue;
		for (size_t i = 0; i < n; i++)
			check::expect_equal(bool(is_prime[i]), bool(expected[n][i]),
			                    "prime_sieve(" + std::to_string(n) + ")[" + std::to_string(i) + "]");
	}
}


static void sieve_matches_trial_division() {
	//
	// The sieve agrees with trial division
	//
	// For every i in [0, n), at bounds on both sides of a square and a prime.

	for (size_t n: {4, 5, 9, 10, 26, 30, 31, 97, 100, 1'000, 10'000}) {
		std::vector<bool> is_prime = prime_sieve(n);
		check::require(is_prime.size() == n, "prime_sieve(" + std::to_string(n) + ") has n entries");

		size_t mismatches = 0;
		for (size_t i = 0; i < n; i++)
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


static std::string displayed_primes(size_t n) {
	//
	// What display_primes(n) writes to standard output

	std::ostringstream out;
	std::streambuf* saved = std::cout.rdbuf(out.rdbuf());
	display_primes(n);
	std::cout.rdbuf(saved);
	return out.str();
}


static void display_lists_the_primes_below_n() {
	//
	// The listing is every prime below n
	//
	// One per line, ascending, from 2, each line ended, and nothing else; empty for n <= 2.

	static const std::vector<std::pair<size_t, std::string>> listing = {
		{0,  ""},
		{1,  ""},
		{2,  ""},
		{3,  "2\n"},
		{4,  "2\n3\n"},
		{5,  "2\n3\n"},
		{6,  "2\n3\n5\n"},
		{30, "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n"},
		{31, "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n"},
		{32, "2\n3\n5\n7\n11\n13\n17\n19\n23\n29\n31\n"},
	};

	for (const auto& [n, text]: listing)
		check::expect_equal(displayed_primes(n), text, "display_primes(" + std::to_string(n) + ") output");
}


static check::Suite
check_by_name {
	{"sieve below four exactly",          sieve_below_four_exactly},
	{"sieve matches trial division",      sieve_matches_trial_division},
	{"sieve marks squares at the bound",  sieve_marks_squares_at_the_bound},
	{"display lists the primes below n",  display_lists_the_primes_below_n},
};
