// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Reference tests
//
// The Lucas-Lehmer test and the base-3 PRP test of M_p = 2^p - 1 on `BigInt`, for small odd p:
// the known answers the real tests and every engine are graded against. They return residues and
// decide nothing; `make check` runs them, and nothing in `mps` calls them.
//

#pragma once


// Internal libs
#include "mp.hpp"


namespace reference {

	// s_(p-2) mod M_p, from s_0 = 4; 0 exactly when M_p is prime. p even or below 3 is an error.
	BigInt lucas_lehmer(long unsigned p);

	// The type-1 residue 3^(M_p - 1) mod M_p; 1 when M_p is a probable prime. p even or below 3 is
	// an error.
	BigInt prp3(long unsigned p);

}
