// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Mathematical operations and tests.
//


// Standard libs
#include <iostream>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Internal libs
#include "math.hpp"
#include "mp.hpp"
#include "log.hpp"
#include "result.hpp"


std::vector<bool> prime_sieve(size_t n) {
	//
	// Primes below n
	//
	// Returns n entries, entry i true exactly when i is prime, by the sieve of Eratosthenes
	// (Crandall and Pomerance, "Prime Numbers: A Computational Perspective", 2nd ed., 2005,
	// ch. 3). Every index written is below n, so for n < 3 the result is n false entries.
	// Only odd multiples of an odd i are struck, starting from i * i: a smaller multiple
	// has a smaller prime factor and is struck already. A composite below n has a prime
	// factor p with p * p < n, so the outer loop stops there.

	std::vector<bool> is_prime(n, false);

	// 2 is the only even prime
	if (n > 2)
		is_prime[2] = true;

	for (size_t i = 3; i < n; i += 2)
		is_prime[i] = true;

	for (size_t i = 3; i * i < n; i += 2)
		if (is_prime[i])
			for (size_t j = i * i; j < n; j += 2 * i)
				is_prime[j] = false;

	return is_prime;
}


void display_primes(size_t n) {
	//
	// Print the primes below n
	//
	// One per line in ascending order, from 2, and nothing else.

	std::vector<bool> is_prime = prime_sieve(n);

	for (size_t i = 2; i < n; i++)
		if (is_prime[i])
			std::cout << i << std::endl;
}


Result test_quick_factor(const long unsigned p) {
	//
	// Trial factoring of M_p
	//
	// Not implemented yet, so it decides nothing and says so.

	logging::info("Running quick factor test");
	return Result::no_verdict(WORK_TF, p, "trial factoring is not implemented yet");
}


Result test_p_minus_one(const long unsigned p) {
	//
	// Pollard's P-1 factoring of M_p
	//
	// Not implemented yet, so it decides nothing and says so.

	logging::info("Running P-1 test");
	return Result::no_verdict(WORK_P1, p, "P-1 is not implemented yet");
}


Result test_lucas_lehmer(const long unsigned p) {
	//
	// The Lucas-Lehmer test of M_p
	//
	// Not implemented yet, so it decides nothing and says so.

	logging::info("Running Lucas-Lehmer test");
	return Result::no_verdict(WORK_LL, p, "Lucas-Lehmer is not implemented yet");
}


Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed) {
	//
	// Run tests on M_p in order, stopping after the first decisive result
	//
	// Each result goes to `completed` as soon as its test returns, before the next test
	// starts. An error decides nothing, so the next test still runs (and the run still fails).
	// Returns the last result handed over.

	std::optional<Result> last;
	for (const TestFunction test: tests) {
		last = test(p);
		completed(*last);
		if (is_decisive(*last))
			break;
	}

	if (not last)
		throw std::logic_error("No test to run");

	return *last;
}


Result test_all(const long unsigned p, const ResultSink& completed) {
	//
	// Trial factoring, P-1 and Lucas-Lehmer on M_p, in that order
	//
	// The cheap tests come first, and the run stops at the first one that decides.

	return run_in_order(p, {test_quick_factor, test_p_minus_one, test_lucas_lehmer}, completed);
}


long unsigned smallest_prime_factor(const long unsigned p) {
	//
	// The smallest prime factor of p, by trial division
	//
	// Returns p itself when p is prime, and 0 when p is below 2 and has no prime factor.
	// Trial division (Crandall and Pomerance, "Prime Numbers: A Computational Perspective",
	// 2nd ed., 2005, ch. 3) tries 2, then every odd d with d * d <= p. The first d that
	// divides p is prime, because a prime factor of a composite d would divide p and be found
	// first. A composite p has a prime factor no larger than its square root, so when no d
	// divides p, p is prime. The bound is written d <= p / d, which cannot overflow.

	if (p < 2)
		return 0;

	if (p % 2 == 0)
		return 2;

	for (long unsigned d = 3; d <= p / d; d += 2)
		if (p % d == 0)
			return d;

	return p;
}


Result composite_by_exponent(const long unsigned p, const long unsigned a) {
	//
	// M_p composite by the factor 2^a - 1, re-checked exactly
	//
	// The only place the exponent check writes a factor, and only once the exact re-check
	// holds. Write p = k * a + r with 0 <= r < a. Since 2^a = 1 (mod 2^a - 1),
	// 2^p = (2^a)^k * 2^r = 2^r (mod 2^a - 1). For a >= 2, 1 <= 2^r <= 2^(a-1) < 2^a - 1, so
	// 2^r is its own residue and is 1 exactly when r = 0. So 2^p = 1 (mod 2^a - 1), the
	// re-check a factor needs before it is reported, holds exactly when a divides p. And
	// 1 < a < p gives 1 < 2^a - 1 < 2^p - 1, a proper factor, so M_p is composite.
	//
	// The guard is evaluated in order, so a = 0 never reaches p % a. It asks only that a be a
	// proper divisor; that a is the smallest prime factor is smallest_prime_factor's to keep.
	// Any other a is an error, never a verdict.

	if (not (1 < a and a < p and p % a == 0))
		return Result::error(WORK_PTEST, p, "the exponent factor " + std::to_string(a) + " fails its exact re-check");

	return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(a) + "-1");
}


std::optional<Result> check_exponent_by(const long unsigned p, FactorSearch search) {
	//
	// The exponent check, with the factor search given
	//
	// The search names a, the smallest prime factor of p, or p itself when p is prime. A
	// composite p makes M_p composite by the factor 2^a - 1; a prime p says nothing about M_p,
	// so it gives no result at all and the tests run. A search can be wrong, so whatever a it
	// names reaches a verdict only through composite_by_exponent's exact re-check: an a that
	// does not divide p, or is 0 or 1, is an error and never a composite. The re-check stands
	// between the search and every verdict this check gives.

	const long unsigned a = search(p);
	if (a == p)
		return std::nullopt;

	return composite_by_exponent(p, a);
}


std::optional<Result> check_exponent(const long unsigned p) {
	//
	// The exponent check: is p prime?
	//
	// A composite p, with smallest prime factor a, makes M_p composite by the factor 2^a - 1,
	// re-checked by composite_by_exponent. A prime p says nothing about M_p, so it gives no
	// result at all and the tests run. PVAL is at least 2 here. The search is trial division,
	// smallest_prime_factor; check_exponent_by holds the check itself.

	return check_exponent_by(p, smallest_prime_factor);
}
