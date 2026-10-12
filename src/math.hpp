// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Mathematical operations and tests.
//

#pragma once


// Standard libs
#include <vector>

// Internal libs
#include "mp.hpp"


std::vector<bool> prime_sieve(size_t n);
void display_primes(size_t n);


bool test_quick_factor(const long unsigned p);
bool test_p_minus_one(const long unsigned p);
bool test_lucas_lehmer(const long unsigned p);
bool test_all(const long unsigned p);

