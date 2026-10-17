// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Reference tests
//
// The Lucas-Lehmer test (Lehmer, "An extended theory of Lucas' functions", 1930) and the base-3
// Fermat test of M_p, written on the reference arithmetic for clarity, not speed. The theorems and
// their hypotheses are in doc/lucas-lehmer.md and doc/prp-and-gerbicz.md.
//


// Standard libs
#include <string>

// Internal libs
#include "mp.hpp"
#include "reference.hpp"


static void require_theorem_domain(long unsigned p) {
	//
	// Both theorems need an odd p of at least 3; any other p is an error

	if (p < 3 or p % 2 == 0)
		throw mp_error("exponent " + std::to_string(p) + " is not odd and at least 3");
}


BigInt reference::lucas_lehmer(long unsigned p) {
	//
	// The Lucas-Lehmer residue s_(p-2) mod M_p
	//
	// From s_0 = 4, p - 2 times s <- (s^2 - 2) mod M_p: square with square, reduce with
	// reduce_mersenne, and add M_p first when the reduced square is below 2, so the difference
	// never goes below zero. The residue lies in [0, M_p), and is 0 exactly when M_p is prime
	// (doc/lucas-lehmer.md, "The result").

	require_theorem_domain(p);
	const BigInt m = mersenne(p);
	const BigInt two(2);
	BigInt s(4);
	for (long unsigned i = 0; i < p - 2; i++) {
		s = reduce_mersenne(square(s), p);
		if (compare(s, two) < 0)
			s = add(s, m);
		s = subtract(s, two);
	}
	return s;
}


BigInt reference::prp3(long unsigned p) {
	//
	// The type-1 residue 3^(M_p - 1) mod M_p
	//
	// M_p - 1 = 2^p - 2 is p - 1 ones and then a zero in binary, so the left-to-right binary powers
	// (MCA, Algorithm 2.12) from x = 3 are p - 2 steps of x <- 3 x^2 mod M_p and a last x <- x^2 mod
	// M_p. It is 1 when M_p is a probable prime; x_p of doc/prp-and-gerbicz.md, Theorem 2, is 9 times
	// it modulo M_p.

	require_theorem_domain(p);
	const BigInt three(3);
	BigInt x(3);
	for (long unsigned i = 0; i < p - 2; i++)
		x = reduce_mersenne(multiply(square(x), three), p);
	return reduce_mersenne(square(x), p);
}
