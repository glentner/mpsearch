// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Mathematical operations and tests.
//

#pragma once


// Standard libs
#include <functional>
#include <optional>
#include <vector>

// Internal libs
#include "mp.hpp"
#include "result.hpp"


std::vector<bool> prime_sieve(size_t n);
void display_primes(size_t n);


// A test, and where a run hands each result as its test completes
using TestFunction = Result (*)(const long unsigned p);
using ResultSink = std::function<void(const Result&)>;


Result test_quick_factor(const long unsigned p);
Result test_p_minus_one(const long unsigned p);
Result test_lucas_lehmer(const long unsigned p);
Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed);
Result test_all(const long unsigned p, const ResultSink& completed);


// The exponent check, run before any test, and the factor search it trusts no further than
// its re-check
using FactorSearch = long unsigned (*)(const long unsigned p);

long unsigned smallest_prime_factor(const long unsigned p);
Result composite_by_exponent(const long unsigned p, const long unsigned a);
std::optional<Result> check_exponent_by(const long unsigned p, FactorSearch search);
std::optional<Result> check_exponent(const long unsigned p);
