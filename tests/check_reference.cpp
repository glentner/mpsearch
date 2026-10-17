// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: reference tests
//
// The reference Lucas-Lehmer and base-3 PRP tests against the Mersenne prime exponents from 3 to
// 10000 (tests/known/mersenne-exponents.txt, read through src/known.*; above 10000 the tests are
// too slow for make check) and against the full residues Python's integers computed for prime
// exponents with M_p composite (tests/vectors/reference.txt). Each check runs every exponent or
// record it covers, prints at most three that disagree, by bit length and low limb, and then
// expects that all of them agree.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "known.hpp"
#include "mp.hpp"
#include "reference.hpp"
#include "vector_reader.hpp"


static void expect_mp_error(const std::function<void()>& call, const std::string& what,
                            std::source_location location = std::source_location::current()) {
	//
	// Expect a call to throw mp_error and return nothing
	//
	// A normal return records "returned a value"; any other exception, "threw <what>".

	try {
		call();
	} catch (mp_error const&) {
		check::record(true, what + " to report an error", location);
		return;
	} catch (std::exception const& error) {
		check::record(false, what + " to report an error", location, std::string("threw ") + error.what());
		return;
	}
	check::record(false, what + " to report an error", location, "returned a value");
}


static const std::string EXPONENTS = "tests/known/mersenne-exponents.txt";

// The largest exponent the reference tests run on in make check; above it they are too slow
static const std::uint64_t REFERENCE_UP_TO = 10'000;


static std::vector<known::Row> listed_primes() {
	//
	// The exponent table's rows from p = 3, the theorems' domain, up to the bound

	std::vector<known::Row> listed;
	try {
		for (const known::Row& row: known::load(EXPONENTS).rows)
			if (row.exponent >= 3 and row.exponent <= REFERENCE_UP_TO)
				listed.push_back(row);
	} catch (known_error const& error) {
		check::require(false, std::string("the exponent table to load (") + error.what() + ")");
	}
	check::require(not listed.empty(), "the exponent table to list an exponent from 3 to the bound");
	return listed;
}


static void expect_listed_residues(const std::function<BigInt(long unsigned)>& test, const BigInt& prime_residue,
                                   const std::string& what, std::source_location location = std::source_location::current()) {
	//
	// Each listed exponent's residue is the residue of a prime, the first three that are not
	// printed by their row

	std::vector<known::Row> listed = listed_primes();
	std::size_t wrong = 0;
	for (const known::Row& row: listed) {
		BigInt residue = test(row.exponent);
		if (not (residue == prime_residue) and ++wrong <= 3)
			std::cout << "   " << EXPONENTS << ":" << row.line << ": got " << vectors::describe(residue) << ", expected "
			          << vectors::describe(prime_residue) << std::endl;
	}
	check::expect(wrong == 0, "all " + std::to_string(listed.size()) + " " + what + " to be a prime's", location);
}


static void lucas_lehmer_ends_in_zero_for_every_listed_prime() {
	//
	// s_(p-2) mod M_p = 0 for every listed exponent from 3 to the bound

	expect_listed_residues(reference::lucas_lehmer, BigInt(), "Lucas-Lehmer residues of listed primes");
}


static void prp_ends_in_one_for_every_listed_prime() {
	//
	// The type-1 residue is 1 for every listed exponent from 3 to the bound

	expect_listed_residues(reference::prp3, BigInt(1), "PRP residues of listed primes");
}


static void expect_composite_residues(const std::string& op, const std::function<BigInt(long unsigned)>& test,
                                      const BigInt& prime_residue, const std::string& what) {
	//
	// Each composite exponent's residue equals its record's, and is not the residue of a prime

	std::vector<const vectors::Record*> records = vectors::records("reference", op);
	vectors::Mismatches mismatches(what);
	for (const vectors::Record* record: records) {
		BigInt residue = test(vectors::number(record->fields[1]));
		if (not vectors::matches(residue, record->fields[2]))
			mismatches.add(*record, "got " + vectors::describe(residue) + ", expected "
			                        + vectors::describe_expected(record->fields[2]));
		else if (residue == prime_residue)
			mismatches.add(*record, "the residue of a composite is a prime's");
	}
	mismatches.expect_none(records.size());
}


static void lucas_lehmer_residues_match_the_vectors() {
	expect_composite_residues("ll", reference::lucas_lehmer, BigInt(), "Lucas-Lehmer residues");
}


static void prp_residues_match_the_vectors() {
	expect_composite_residues("prp", reference::prp3, BigInt(1), "PRP residues");
}


static void the_reference_tests_refuse_exponents_outside_their_theorems() {
	//
	// Both theorems need an odd p of at least 3: 0, 1, 2 and even p are errors

	for (long unsigned p: {0ul, 1ul, 2ul, 4ul, 10ul}) {
		std::string exponent = std::to_string(p);
		expect_mp_error([p] { reference::lucas_lehmer(p); }, "reference::lucas_lehmer(" + exponent + ")");
		expect_mp_error([p] { reference::prp3(p); }, "reference::prp3(" + exponent + ")");
	}
}


static check::Suite
check_by_name {
	{"Lucas-Lehmer ends in zero for every listed prime",             lucas_lehmer_ends_in_zero_for_every_listed_prime},
	{"Lucas-Lehmer residues match the vectors",                      lucas_lehmer_residues_match_the_vectors},
	{"PRP ends in one for every listed prime",                       prp_ends_in_one_for_every_listed_prime},
	{"PRP residues match the vectors",                               prp_residues_match_the_vectors},
	{"the reference tests refuse exponents outside their theorems",  the_reference_tests_refuse_exponents_outside_their_theorems},
};
