// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Known answers
//
// The loader of the tables in tests/known/: the Mersenne prime exponents, the final residues of
// composite exponents, and the residues after a stated number of iterations, fingerprints among
// them. Every row carries its source. A table is read whole or refused whole: the loader checks
// every rule of the format, and the first rule a table breaks names the table and the row.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "exc.hpp"


// A table that breaks a rule of its format, "<path>:<line>: <reason>", or a file that does not
// open, "<path>: does not open"
class known_error: public mps_error {
public:
	known_error(const std::string& msg): mps_error(msg) {}

};


namespace known {


	// The tables' paths from the repository root: exponents, final residues, partial residues
	extern const std::vector<std::string> TABLES;


	// Where a row's answer comes from: a published result, the table's own command, or the agreeing
	// runs of two or more engines (a fingerprint)
	enum class Origin {
		PUBLISHED,
		COMPUTED,
		RUNS,
	};


	// A source line, the source of every row below it up to the next one
	struct Source {
		Origin origin;
		std::string reference;  // published: its key in doc/references.md
		std::string retrieved;  // published: the date it was retrieved, YYYY-MM-DD
		std::string text;       // what the line says after its colon
		std::size_t line;
	};


	// One of a fingerprint's agreeing runs
	struct Run {
		std::string engine;
		std::uint64_t length;
		std::uint64_t shift;
	};


	struct Row {
		std::size_t line = 0;
		std::string op;                 // mersenne; ll or prp1 (final); ll or prp (partial)
		std::uint64_t exponent = 0;
		std::uint64_t start = 0;        // partial residues only
		std::uint64_t iterations = 0;   // partial residues only
		std::string res64;              // empty for an exponent
		std::uint64_t agreed = 0;       // published final residues: how many results agreed
		std::vector<Run> runs;          // fingerprints only
		std::string supersedes;         // the Res64 of the earlier row of its key it replaces
		std::string reason;             // why it replaces that row
		std::size_t source = 0;         // its source's index in Table::sources
		bool superseded = false;        // a later row replaces it
	};


	struct Table {
		std::string path;
		std::string stem;
		std::vector<std::string> notes;
		std::vector<Source> sources;
		std::vector<Row> rows;
	};


	// The table in text, read from path; known_error at the first rule it breaks. The basename of
	// path is the table's own, <stem>.txt.
	Table parse(const std::string& path, const std::string& text);


	// The table in the file at path, from the working directory, read by parse
	Table load(const std::string& path);

}
