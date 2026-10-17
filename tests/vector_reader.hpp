// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Vector reader
//
// Reads the vector families under `tests/vectors/`, which `tests/vectors/generate.py` wrote with
// Python's integers, relative to the working directory (`make check` runs `bin/check` from the
// repository root). Loading a family `require`s its envelope: the magic line, the command that
// reproduces it, its sources, known ops with well-formed fields, and the trailer's count and
// SHA-256. Operands are rebuilt into limbs with `BigInt::from_limbs`, never through the parsers
// under check. SHA-256 (FIPS 180-4) and SplitMix64 (Steele, Lea and Flood 2014) live here too.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "mp.hpp"


namespace vectors {


	// Every family the checks read
	extern const std::vector<std::string> STEMS;


	struct Record {
		std::string file;
		std::size_t line;
		std::vector<std::string> fields;  // fields[0] is the op

		std::string where() const;
	};


	struct Family {
		std::string stem;
		std::string command;
		std::vector<std::string> sources;
		std::vector<Record> records;
	};


	const Family& load(const std::string& stem);


	std::vector<const Record*> records(const std::string& stem, const std::string& op);


	std::string sha256(const std::string& bytes);


	class SplitMix64 {
	public:
		explicit SplitMix64(std::uint64_t seed);
		std::uint64_t next();

	private:
		std::uint64_t state;
	};


	BigInt operand(const std::string& field);


	std::uint64_t number(const std::string& field);


	bool matches(const BigInt& value, const std::string& field);


	bool matches_string(const std::string& text, const std::string& field);


	std::string describe(const BigInt& value);


	std::string describe_expected(const std::string& field);


	class Mismatches {
	public:
		explicit Mismatches(const std::string& what);
		void add(const Record& record, const std::string& detail);
		void expect_none(std::size_t total, std::source_location location = std::source_location::current());

	private:
		std::string what;
		std::size_t count = 0;
	};

}
