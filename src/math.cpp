// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Mathematical operations and tests.
//


// Standard libs
#include <iostream>
#include <cstdint>
#include <vector>

// Internal libs
#include "math.hpp"
#include "mp.hpp"
#include "log.hpp"


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


bool test_quick_factor([[maybe_unused]] const long unsigned p) {

	logging::info("Running quick factor test");
	return true;
}


bool test_p_minus_one([[maybe_unused]] const long unsigned p) {

	logging::info("Running P-1 test");
	return true;
}


bool test_lucas_lehmer([[maybe_unused]] const long unsigned p) {

	logging::info("Running Lucas-Lehmer test");
	return true;
}


bool test_all(const long unsigned p) {

	return test_quick_factor(p) and
		   test_p_minus_one(p) and
		   test_lucas_lehmer(p);
}

