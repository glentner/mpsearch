// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: reference tests
//
// The reference Lucas-Lehmer and base-3 PRP tests against the prime list
// (tests/vectors/mersenne-exponents.txt, the Mersenne prime exponents with their published source)
// and against the full residues Python's integers computed for prime exponents with M_p composite
// (tests/vectors/reference.txt). Each check runs every record of its ops, prints at most three
// that disagree, by bit length and low limb, and then expects that all of them agree.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
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


static std::vector<const vectors::Record*> listed_primes() {
	//
	// The prime list's records from p = 3 on, the theorems' domain

	std::vector<const vectors::Record*> listed;
	for (const vectors::Record* record: vectors::records("mersenne-exponents", "mersenne"))
		if (vectors::number(record->fields[1]) >= 3)
			listed.push_back(record);
	check::require(not listed.empty(), "the prime list has an exponent from 3 on");
	return listed;
}


static void the_prime_list_carries_its_source() {
	//
	// The prime list's command and its published source, OEIS-A000043, and its exponents ascending
	// from 2

	const std::string stem = "mersenne-exponents";
	const vectors::Family& family = vectors::load(stem);
	check::expect_equal(family.command,
	                    "# command: python3 tests/vectors/generate.py " + stem + " > tests/vectors/" + stem + ".txt",
	                    "the prime list to carry its command");
	bool named = false;
	for (const std::string& source: family.sources)
		if (source.find("OEIS-A000043") != std::string::npos)
			named = true;
	check::expect(named, "a source line of the prime list to name OEIS-A000043");

	std::vector<const vectors::Record*> records = vectors::records(stem, "mersenne");
	check::expect_equal(vectors::number(records.front()->fields[1]), std::uint64_t(2), "the prime list to start at 2");
	bool ascending = true;
	for (std::size_t i = 1; i < records.size(); i++)
		if (vectors::number(records[i]->fields[1]) <= vectors::number(records[i - 1]->fields[1]))
			ascending = false;
	check::expect(ascending, "the prime list to ascend");
}


static void lucas_lehmer_ends_in_zero_for_every_listed_prime() {
	//
	// s_(p-2) mod M_p = 0 for every listed exponent from 3 on

	std::vector<const vectors::Record*> listed = listed_primes();
	vectors::Mismatches mismatches("Lucas-Lehmer residues of listed primes");
	for (const vectors::Record* record: listed) {
		BigInt s = reference::lucas_lehmer(vectors::number(record->fields[1]));
		if (not s.is_zero())
			mismatches.add(*record, "got " + vectors::describe(s) + ", expected 0");
	}
	mismatches.expect_none(listed.size());
}


static void prp_ends_in_one_for_every_listed_prime() {
	//
	// The type-1 residue is 1 for every listed exponent from 3 on

	std::vector<const vectors::Record*> listed = listed_primes();
	vectors::Mismatches mismatches("PRP residues of listed primes");
	for (const vectors::Record* record: listed) {
		BigInt x = reference::prp3(vectors::number(record->fields[1]));
		if (not (x == BigInt(1)))
			mismatches.add(*record, "got " + vectors::describe(x) + ", expected 1");
	}
	mismatches.expect_none(listed.size());
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
	{"the prime list carries its source",                            the_prime_list_carries_its_source},
	{"Lucas-Lehmer ends in zero for every listed prime",             lucas_lehmer_ends_in_zero_for_every_listed_prime},
	{"Lucas-Lehmer residues match the vectors",                      lucas_lehmer_residues_match_the_vectors},
	{"PRP ends in one for every listed prime",                       prp_ends_in_one_for_every_listed_prime},
	{"PRP residues match the vectors",                               prp_residues_match_the_vectors},
	{"the reference tests refuse exponents outside their theorems",  the_reference_tests_refuse_exponents_outside_their_theorems},
};
