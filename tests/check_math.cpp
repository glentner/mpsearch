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
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Internal libs
#include "check.hpp"
#include "math.hpp"
#include "result.hpp"


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


static void the_stubs_decide_nothing() {
	//
	// Each test, not implemented yet, says so and decides nothing
	//
	// For prime and composite exponents alike: a no verdict naming its test, the exponent and
	// why, after the test's start line on stderr.

	struct Stub {
		TestFunction test;
		Worktype worktype;
		std::string reason;
		std::string start;
	};
	static const std::vector<Stub> stubs = {
		{test_quick_factor,  WORK_TF,  "trial factoring is not implemented yet",  "Running quick factor test"},
		{test_p_minus_one,   WORK_P1,  "P-1 is not implemented yet",              "Running P-1 test"},
		{test_lucas_lehmer,  WORK_LL,  "Lucas-Lehmer is not implemented yet",     "Running Lucas-Lehmer test"},
	};

	for (long unsigned p: {2ul, 3ul, 4ul, 31ul, 33ul, 999'999'937ul, 999'999'999ul})
		for (const Stub& stub: stubs) {
			std::ostringstream err;
			std::streambuf* saved = std::cerr.rdbuf(err.rdbuf());
			const Result result = stub.test(p);
			std::cerr.rdbuf(saved);

			const std::string label = name_by_worktype[stub.worktype] + " on " + std::to_string(p);
			check::expect(result.outcome() == OUTCOME_NO_VERDICT, label + " gives no verdict");
			check::expect(not is_decisive(result), label + " is not decisive");
			check::expect(result.worktype() == stub.worktype, label + " names its test");
			check::expect_equal(result.exponent(), p, label + " names its exponent");
			check::expect_equal(result.reason(), stub.reason, label + " says why");
			check::expect(err.str().find(stub.start) != std::string::npos, label + " logs its start line");
		}
}


static void smallest_prime_factor_matches_the_sieve() {
	//
	// The smallest prime factor agrees with the sieve
	//
	// For every n below 100000: n itself exactly when the sieve marks n prime, and otherwise
	// the least index the sieve marks prime that divides n. Below 2 there is no prime factor.

	static const size_t bound = 100'000;
	std::vector<bool> is_prime = prime_sieve(bound);
	check::require(is_prime.size() == bound, "prime_sieve(" + std::to_string(bound) + ") has n entries");

	size_t mismatches = 0;
	for (size_t n = 2; n < bound; n++) {
		size_t least = n;
		if (not is_prime[n])
			for (size_t i = 2; i < n; i++)
				if (is_prime[i] and n % i == 0) {
					least = i;
					break;
				}
		if (smallest_prime_factor(n) != least)
			mismatches++;
	}
	check::expect_equal(mismatches, size_t(0), "smallest_prime_factor(n) matches the sieve below " + std::to_string(bound));
	check::expect_equal(smallest_prime_factor(0), 0ul, "smallest_prime_factor(0) is 0");
	check::expect_equal(smallest_prime_factor(1), 0ul, "smallest_prime_factor(1) is 0");
}


// Known smallest prime factors, the composites first: the prime squares, where the search
// must try d with d * d = p; the bound's neighbours; and the largest smallest prime factor a
// PVAL can have, 31607, whose square is the last composite trial division must reach. Each
// row's command recomputes it.
static const std::vector<std::pair<long unsigned, long unsigned>> known_factors = {
	{4,            2},       // python3 -c 'n = 4; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{9,            3},       // python3 -c 'n = 9; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{25,           5},       // python3 -c 'n = 25; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{33,           3},       // python3 -c 'n = 33; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{49,           7},       // python3 -c 'n = 49; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{961,          31},      // python3 -c 'n = 961; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{999'999'999,  3},       // python3 -c 'n = 999999999; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{999'999'998,  2},       // python3 -c 'n = 999999998; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{998'623'201,  31'601},  // python3 -c 'n = 998623201; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{998'812'807,  31'601},  // python3 -c 'n = 998812807; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{999'002'449,  31'607},  // python3 -c 'n = 999002449; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{2,            2},       // python3 -c 'n = 2; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{3,            3},       // python3 -c 'n = 3; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{31,           31},      // python3 -c 'n = 31; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{999'999'929,  999'999'929},  // python3 -c 'n = 999999929; print(next((d for d in range(2, 31623) if n % d == 0), n))'
	{999'999'937,  999'999'937},  // python3 -c 'n = 999999937; print(next((d for d in range(2, 31623) if n % d == 0), n))'
};


static void smallest_prime_factor_known_answers() {
	//
	// The smallest prime factor of each known answer

	for (const auto& [p, a]: known_factors)
		check::expect_equal(smallest_prime_factor(p), a, "smallest_prime_factor(" + std::to_string(p) + ")");
}


static std::string factors_of(const Result& result) {
	//
	// A result's factors, as one comma-separated string

	std::string text;
	for (const std::string& factor: result.factors())
		text += (text.empty() ? "" : ", ") + factor;
	return text;
}


static void the_exponent_check_answers_composites() {
	//
	// A composite exponent is answered; a prime one is not
	//
	// For each known answer: a composite p gives one composite result from the exponent
	// check, for M_p, with the factor 2^a-1 for its smallest prime factor a; a prime p gives
	// no result at all, never a prime and never a no verdict.

	for (const auto& [p, a]: known_factors) {
		const std::optional<Result> result = check_exponent(p);
		const std::string label = "check_exponent(" + std::to_string(p) + ")";
		if (a == p) {
			check::expect(not result.has_value(), label + " gives no result for a prime exponent");
			continue;
		}
		check::require(result.has_value(), label + " answers a composite exponent");
		check::expect(result->outcome() == OUTCOME_COMPOSITE, label + " is composite");
		check::expect(result->worktype() == WORK_PTEST, label + " names the exponent check");
		check::expect_equal(result->exponent(), p, label + " names its exponent");
		check::expect_equal(factors_of(*result), "2^" + std::to_string(a) + "-1", label + " names 2^a-1");
	}
}


static void the_re_check_refuses_a_non_divisor() {
	//
	// The exponent factor is reported only when it re-checks
	//
	// composite_by_exponent(p, a) gives a composite only for 1 < a < p with a dividing p: an
	// a of 0 or 1, a non-divisor, p itself or a multiple of it is an error and never a verdict.
	// A proper divisor that is not the smallest prime factor still re-checks: the guard asks
	// what a reported factor needs, and the search keeps a the smallest.

	for (const auto& [p, a]: std::vector<std::pair<long unsigned, long unsigned>>{
	         {33, 0}, {33, 1}, {33, 5}, {33, 33}, {33, 66}, {4, 4}, {2, 1}, {1, 1}, {0, 0}}) {
		const Result result = composite_by_exponent(p, a);
		const std::string label = "composite_by_exponent(" + std::to_string(p) + ", " + std::to_string(a) + ")";
		check::expect(result.outcome() == OUTCOME_ERROR, label + " is an error");
		check::expect(not is_decisive(result), label + " decides nothing");
		check::expect(result.factors().empty(), label + " names no factor");
	}

	for (const auto& [p, a]: std::vector<std::pair<long unsigned, long unsigned>>{{33, 11}, {33, 3}, {999'002'449, 31'607}}) {
		const Result result = composite_by_exponent(p, a);
		const std::string label = "composite_by_exponent(" + std::to_string(p) + ", " + std::to_string(a) + ")";
		check::expect(result.outcome() == OUTCOME_COMPOSITE, label + " is composite");
		check::expect_equal(factors_of(result), "2^" + std::to_string(a) + "-1", label + " names 2^a-1");
	}
}


// Factor searches that lie about 33, whose smallest prime factor is 3: none of 0, 1 and 5 is
// a proper divisor of 33, and 11 is one but not the smallest.
static long unsigned search_says_zero(const long unsigned) { return 0; }
static long unsigned search_says_one(const long unsigned) { return 1; }
static long unsigned search_says_five(const long unsigned) { return 5; }
static long unsigned search_says_eleven(const long unsigned) { return 11; }


static void the_exponent_check_re_checks_its_search() {
	//
	// The exponent check reports a factor only through its re-check
	//
	// check_exponent_by trusts its factor search no further than composite_by_exponent's
	// exact re-check: a search that names 0, 1 or a non-divisor of 33 gives an error, never a
	// composite, and one that names the larger divisor 11 gives that factor, which re-checks.
	// Under the true search, trial division, it answers exactly as check_exponent does.

	for (const auto& [search, said]: std::vector<std::pair<FactorSearch, std::string>>{
	         {search_says_zero, "0"}, {search_says_one, "1"}, {search_says_five, "5"}}) {
		const std::optional<Result> result = check_exponent_by(33, search);
		const std::string label = "check_exponent_by(33) with a search that says " + said;
		check::require(result.has_value(), label + " answers");
		check::expect(result->outcome() == OUTCOME_ERROR, label + " is an error");
		check::expect(not is_decisive(*result), label + " decides nothing");
		check::expect(result->factors().empty(), label + " names no factor");
	}

	const std::optional<Result> larger = check_exponent_by(33, search_says_eleven);
	check::require(larger.has_value(), "check_exponent_by(33) with a search that says 11 answers");
	check::expect(larger->outcome() == OUTCOME_COMPOSITE, "check_exponent_by(33) with a search that says 11 is composite");
	check::expect_equal(factors_of(*larger), std::string("2^11-1"), "check_exponent_by(33) with a search that says 11 names 2^11-1");

	for (const auto& [p, a]: known_factors) {
		const std::optional<Result> by_search = check_exponent_by(p, smallest_prime_factor);
		const std::optional<Result> checked = check_exponent(p);
		const std::string label = "check_exponent_by(" + std::to_string(p) + ") under trial division";
		check::expect_equal(by_search.has_value(), checked.has_value(), label + " answers as check_exponent does");
		if (not by_search or not checked)
			continue;
		check::expect(by_search->outcome() == checked->outcome(), label + " has check_exponent's outcome");
		check::expect_equal(factors_of(*by_search), factors_of(*checked), label + " names check_exponent's factor");
	}
}


static check::Suite
check_by_name {
	{"sieve below four exactly",          sieve_below_four_exactly},
	{"sieve matches trial division",      sieve_matches_trial_division},
	{"sieve marks squares at the bound",  sieve_marks_squares_at_the_bound},
	{"display lists the primes below n",  display_lists_the_primes_below_n},
	{"the stubs decide nothing",          the_stubs_decide_nothing},
	{"smallest prime factor matches the sieve",  smallest_prime_factor_matches_the_sieve},
	{"smallest prime factor known answers",      smallest_prime_factor_known_answers},
	{"the exponent check answers composites",    the_exponent_check_answers_composites},
	{"the re-check refuses a non-divisor",       the_re_check_refuses_a_non_divisor},
	{"the exponent check re-checks its search",  the_exponent_check_re_checks_its_search},
};
