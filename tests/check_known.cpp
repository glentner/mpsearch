// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: known answers
//
// The loader of tests/known/ (src/known.*) on tables of these checks' own: each kind of table read
// as its text gives it, and every rule of the format shown refused, at the line that breaks it, on
// a table that breaks only that rule. Then the tables themselves: each loads, each published source
// has its bibliography entry, the exponents cite the published list exactly above the bound the
// table's command recomputes up to, and each final residue is of a composite Mersenne number and
// agrees with the full residue tests/vectors/reference.txt holds for its test and exponent.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "known.hpp"
#include "mp.hpp"
#include "vector_reader.hpp"


static const std::string EXPONENTS = "tests/known/mersenne-exponents.txt";
static const std::string FINAL = "tests/known/final-residues.txt";
static const std::string PARTIAL = "tests/known/partial-residues.txt";

// The table's command recomputes the exponents up to this bound; above it they cite the list
static const std::uint64_t RECOMPUTED_UP_TO = 10'000;


static std::string path_of(const std::string& stem) {
	return "tests/known/" + stem + ".txt";
}


static std::string table_text(const std::string& stem, const std::vector<std::string>& body) {
	//
	// A table of these checks' own
	//
	// The magic line, the command for its stem, the body, then the trailer counting the body's
	// rows. The body's first line is line 3 of the table, and the trailer follows its last.

	std::string text = "# mps known 1\n# command: python3 tests/known/generate.py " + stem + " > tests/known/" + stem + ".txt\n";
	std::size_t rows = 0;
	for (const std::string& line: body) {
		text += line + "\n";
		if (line.empty() or line[0] != '#')
			rows++;
	}
	return text + "# end " + std::to_string(rows) + " rows\n";
}


static std::vector<std::string> with(std::vector<std::string> body, std::size_t index, const std::string& line) {
	//
	// The body with its line at index replaced, or with the line appended when index is its size

	if (index == body.size())
		body.push_back(line);
	else
		body[index] = line;
	return body;
}


static std::vector<std::string> without(std::vector<std::string> body, std::size_t index) {
	body.erase(body.begin() + std::ptrdiff_t(index));
	return body;
}


// One body per kind of table, each read by `the loader reads a table it is given`, and changed one
// line at a time by the refusal checks

static const std::vector<std::string> EXPONENT_BODY = {
	"# a note",
	"# from computed: a sweep",
	"mersenne 2",
	"mersenne 3",
	"# from published GIMPSPrimes retrieved 2026-10-17: the list",
	"mersenne 11213",
};

static const std::vector<std::string> FINAL_BODY = {
	"# from published GIMPSReportLL retrieved 2026-10-17: a report",
	"ll 1237 DF5E6622F83F5BE2 agreed=2",
	"prp1 28603 F899E5F1E2B40323 agreed=3",
	"# from computed: this command",
	"ll 1277 5613A480590E78BA",
};

static const std::vector<std::string> PARTIAL_BODY = {
	"# from computed: this command",
	"ll 1277 4 100 EE3E8EF89BAFBDCD",
	"prp 1277 3 100 C6D7DFE3FCAEDC31",
	"# from runs: two engines",
	"ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=ntt:72:5",
};


static void expect_refused(const std::string& stem, const std::string& text, std::size_t line, const std::string& what,
                           std::source_location location = std::source_location::current()) {
	//
	// Expect the loader to refuse a table with a message naming the table and the line

	std::string named = path_of(stem) + ":" + std::to_string(line) + ": ";
	try {
		known::parse(path_of(stem), text);
	} catch (known_error const& error) {
		std::string message = error.what();
		check::record(message.rfind(named, 0) == 0, what + " to be refused at " + named, location, "refused: " + message);
		return;
	}
	check::record(false, what + " to be refused at " + named, location, "read without a refusal");
}


static known::Table must_read(const std::string& stem, const std::string& text,
                              std::source_location location = std::source_location::current()) {
	//
	// A table of these checks' own that the loader must read; a refusal ends the check

	try {
		return known::parse(path_of(stem), text);
	} catch (known_error const& error) {
		check::require(false, std::string("a valid table to be read (") + error.what() + ")", location);
	}
	return {};
}


static known::Table must_load(const std::string& path, std::source_location location = std::source_location::current()) {
	//
	// One of the tables under tests/known/; a refusal ends the check

	try {
		return known::load(path);
	} catch (known_error const& error) {
		check::require(false, std::string(error.what()), location);
	}
	return {};
}


static void the_loader_reads_a_table_it_is_given() {
	//
	// Each kind of table read as its text gives it: notes, sources, rows, fields and attributes

	known::Table exponents = must_read("mersenne-exponents", table_text("mersenne-exponents", EXPONENT_BODY));
	check::expect_equal(exponents.path, path_of("mersenne-exponents"), "the exponent table's path");
	check::expect_equal(exponents.stem, std::string("mersenne-exponents"), "the exponent table's stem");
	check::expect(exponents.notes == std::vector<std::string>({"a note"}), "the exponent table's note");
	check::require(exponents.rows.size() == 3 and exponents.sources.size() == 2, "three exponents under two sources");
	check::expect(exponents.rows[0].op == "mersenne" and exponents.rows[0].exponent == 2 and exponents.rows[0].line == 5,
	              "mersenne 2 on line 5");
	check::expect(exponents.rows[2].exponent == 11213 and exponents.rows[2].line == 8 and exponents.rows[2].res64.empty(),
	              "mersenne 11213 on line 8, with no Res64");
	const known::Source& sweep = exponents.sources[exponents.rows[1].source];
	check::expect(sweep.origin == known::Origin::COMPUTED and sweep.text == "a sweep" and sweep.line == 4,
	              "mersenne 3 to be computed, by the source on line 4");
	const known::Source& list = exponents.sources[exponents.rows[2].source];
	check::expect(list.origin == known::Origin::PUBLISHED and list.reference == "GIMPSPrimes" and list.retrieved == "2026-10-17"
	              and list.text == "the list", "mersenne 11213 to cite GIMPSPrimes, retrieved 2026-10-17");

	known::Table final = must_read("final-residues", table_text("final-residues", FINAL_BODY));
	check::require(final.rows.size() == 3, "three final residues");
	const known::Row& prp = final.rows[1];
	check::expect(prp.op == "prp1" and prp.exponent == 28603 and prp.res64 == "F899E5F1E2B40323" and prp.agreed == 3,
	              "prp1 28603's fields and agreement count");
	check::expect(final.rows[2].agreed == 0 and final.sources[final.rows[2].source].origin == known::Origin::COMPUTED,
	              "the computed row to carry no agreement count");

	known::Table partial = must_read("partial-residues", table_text("partial-residues", PARTIAL_BODY));
	check::require(partial.rows.size() == 3, "three partial residues");
	const known::Row& computed = partial.rows[1];
	check::expect(computed.op == "prp" and computed.start == 3 and computed.iterations == 100 and computed.runs.empty(),
	              "prp 1277 3 100's key, and no runs");
	const known::Row& fingerprint = partial.rows[2];
	check::expect(fingerprint.start == 4 and fingerprint.iterations == 100 and fingerprint.runs.size() == 2
	              and fingerprint.runs[0].engine == "fp64" and fingerprint.runs[1].engine == "ntt"
	              and fingerprint.runs[1].length == 72 and fingerprint.runs[1].shift == 5, "the fingerprint's key and runs");
	check::expect(partial.sources[fingerprint.source].origin == known::Origin::RUNS, "the fingerprint's source to be its runs");
	check::expect(not partial.rows[0].superseded and not fingerprint.superseded, "no row replaced");
}


static void the_loader_refuses_a_malformed_table() {
	//
	// The envelope, each source form, each field and each attribute, refused at the line that
	// breaks it; then the exponents' order, and a final residue in the partial table

	const std::string f = "final-residues";
	std::string good = table_text(f, FINAL_BODY);
	std::string trailer = "# end 3 rows\n";
	std::string before_trailer = good.substr(0, good.size() - trailer.size());
	expect_refused(f, "", 1, "an empty text");
	expect_refused(f, good.substr(0, good.size() - 1), 8, "a table that does not end in LF");
	expect_refused(f, "# mps known 2" + good.substr(13), 1, "another magic line");
	expect_refused(f, "# mps known 1\n# end 0 rows\n", 2, "a table of two lines");
	expect_refused(f, table_text("partial-residues", FINAL_BODY), 2, "the command of another table");
	expect_refused(f, before_trailer + "# end 4 rows\n", 8, "a trailer counting other rows");
	expect_refused(f, before_trailer, 7, "a table without its trailer");
	expect_refused(f, good + "ll 1279 0123456789ABCDEF\n", 9, "a row after the trailer");
	expect_refused(f, table_text(f, with(FINAL_BODY, 2, "# end 1 rows")), 5, "a trailer before the last line");
	expect_refused(f, table_text(f, with(FINAL_BODY, 3, "# a note after a source")), 6, "a note after a source line");
	expect_refused(f, table_text(f, with(FINAL_BODY, 3, "#from computed: this command")), 6, "a '#' line without its space");
	expect_refused(f, table_text(f, with(FINAL_BODY, 3, "")), 6, "an empty line");

	expect_refused(f, table_text(f, with(FINAL_BODY, 0, "# from published GIMPS_ReportLL retrieved 2026-10-17: x")), 3,
	               "a key with '_'");
	expect_refused(f, table_text(f, with(FINAL_BODY, 0, "# from published GIMPSReportLL retrieved 2026-13-17: x")), 3,
	               "a month 13");
	expect_refused(f, table_text(f, with(FINAL_BODY, 0, "# from published GIMPSReportLL: x")), 3, "a published source with no date");
	expect_refused(f, table_text(f, with(FINAL_BODY, 3, "# from computed:")), 6, "a source that says nothing");
	expect_refused(f, table_text(f, with(FINAL_BODY, 3, "# from recalled: x")), 6, "an unknown kind of source");

	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 DF5E6622F83F5BE agreed=2")), 4, "a Res64 of 15 digits");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 DF5E6622F83F5BE20 agreed=2")), 4, "a Res64 of 17 digits");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 df5e6622f83f5be2 agreed=2")), 4, "a Res64 in lowercase");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 01237 DF5E6622F83F5BE2 agreed=2")), 4, "an exponent with a leading zero");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1239 DF5E6622F83F5BE2 agreed=2")), 4, "an exponent that is not prime");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 4294967311 DF5E6622F83F5BE2 agreed=2")), 4, "a prime exponent above 2^32");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 2 DF5E6622F83F5BE2 agreed=2")), 4, "a residue of the exponent 2");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "prp 1237 DF5E6622F83F5BE2 agreed=2")), 4, "an op of another table");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 agreed=2")), 4, "a missing field");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll  1237 DF5E6622F83F5BE2 agreed=2")), 4, "a double space");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll\t1237 DF5E6622F83F5BE2 agreed=2")), 4, "a tab");

	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 DF5E6622F83F5BE2 agreed=1")), 4, "one agreeing result");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 DF5E6622F83F5BE2")), 4, "a published final residue without agreed=");
	expect_refused(f, table_text(f, with(FINAL_BODY, 4, "ll 1277 5613A480590E78BA agreed=2")), 7, "agreed= on a computed row");
	expect_refused(f, table_text(f, with(FINAL_BODY, 1, "ll 1237 DF5E6622F83F5BE2 agreed=2 shift=3")), 4, "an unknown attribute");
	expect_refused(f, table_text(f, with(FINAL_BODY, 4, "ll 1277 5613A480590E78BA supersedes=5613A480590E78BA")), 7,
	               "a correction that does not say why");
	expect_refused(f, table_text(f, with(FINAL_BODY, 4, "ll 1277 5613A480590E78BA because no reason")), 7,
	               "a reason with nothing replaced");

	const std::string e = "mersenne-exponents";
	expect_refused(e, table_text(e, with(EXPONENT_BODY, 3, "mersenne 2")), 6, "an exponent twice");
	expect_refused(e, table_text(e, with(EXPONENT_BODY, 6, "mersenne 9941")), 9, "exponents out of order");
	expect_refused(e, table_text(e, with(EXPONENT_BODY, 3, "mersenne 9")), 6, "an exponent that is not prime");

	const std::string p = "partial-residues";
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 1, "ll 1277 4 0 EE3E8EF89BAFBDCD")), 4, "zero iterations");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=ntt:72:1277")), 7,
	               "a shift of p");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64 run=ntt:72:5")), 7,
	               "a run without its shift");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 1, "ll 1277 4 1275 5613A480590E78BA")), 4,
	               "the final Lucas-Lehmer residue in the partial table");
}


static void the_loader_refuses_a_row_without_a_source() {
	//
	// A row above every source line has no source, in each kind of table

	expect_refused("mersenne-exponents", table_text("mersenne-exponents", without(EXPONENT_BODY, 1)), 4,
	               "an exponent below a note and above every source line");
	expect_refused("final-residues", table_text("final-residues", without(FINAL_BODY, 0)), 3,
	               "a final residue above every source line");
	expect_refused("partial-residues", table_text("partial-residues", without(PARTIAL_BODY, 0)), 3,
	               "a partial residue above every source line");
}


static void the_loader_refuses_two_answers_for_one_key() {
	//
	// Two rows of one key with different Res64s, neither replaced, refused at the later one, a
	// fingerprint against a computed row among them; two rows that agree are read

	const std::string f = "final-residues";
	expect_refused(f, table_text(f, with(FINAL_BODY, 5, "ll 1237 DF5E6622F83F5BE3")), 8, "two answers for ll 1237");
	known::Table agreeing = must_read(f, table_text(f, with(FINAL_BODY, 5, "ll 1237 DF5E6622F83F5BE2")));
	check::expect_equal(agreeing.rows.size(), std::size_t(4), "two rows of ll 1237 that agree to be read");
	const std::string p = "partial-residues";
	std::vector<std::string> again = PARTIAL_BODY;
	again.insert(again.end(), {"# from computed: again", "ll 1277 4 100 EE3E8EF89BAFBDCE"});
	expect_refused(p, table_text(p, again), 9, "two answers for ll 1277 4 100");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 0123456789ABCDEF run=fp64:64:0 run=ntt:72:5")), 7,
	               "a fingerprint against a computed row");
}


static void the_loader_honours_supersession() {
	//
	// A correction replaces the earliest row above it of its key that gives the Res64 it names
	// and that no other row replaced; it keeps its reason; one that names no such row is refused,
	// and so is a computed correction of a published final residue the command does not recompute

	const std::string f = "final-residues";
	std::vector<std::string> body = with(FINAL_BODY, 5, "ll 1237 0123456789ABCDEF supersedes=DF5E6622F83F5BE2 because a test");
	known::Table corrected = must_read(f, table_text(f, body));
	check::require(corrected.rows.size() == 4, "four final residues");
	check::expect(corrected.rows[0].superseded and not corrected.rows[3].superseded, "ll 1237's first row to be replaced");
	check::expect(corrected.rows[3].supersedes == "DF5E6622F83F5BE2" and corrected.rows[3].reason == "a test",
	              "the correction to keep what it replaces and why");
	body.push_back("ll 1237 DF5E6622F83F5BE2 supersedes=0123456789ABCDEF because back again");
	known::Table back = must_read(f, table_text(f, body));
	check::require(back.rows.size() == 5, "five final residues");
	check::expect(back.rows[0].superseded and back.rows[3].superseded and not back.rows[4].superseded,
	              "a chain of corrections, away and back");

	expect_refused(f, table_text(f, with(FINAL_BODY, 5, "ll 1237 0123456789ABCDEF supersedes=FFFFFFFFFFFFFFFF because a test")), 8,
	               "a correction naming no row");
	expect_refused(f, table_text(f, with(FINAL_BODY, 5, "ll 1277 0123456789ABCDEF supersedes=DF5E6622F83F5BE2 because a test")), 8,
	               "a correction naming a row of another key");
	std::vector<std::string> above = FINAL_BODY;
	above.push_back("ll 1279 1111111111111111 supersedes=0123456789ABCDEF because a test");
	above.push_back("ll 1279 0123456789ABCDEF");
	expect_refused(f, table_text(f, above), 8, "a correction naming a row only below it");

	std::vector<std::string> large = FINAL_BODY;
	large.insert(large.end(), {
		"# from published GIMPSReportLL retrieved 2026-10-17: a report",
		"ll 1000193 8F5354551BC18373 agreed=5",
		"# from computed: this command",
		"ll 1000193 0123456789ABCDEF supersedes=8F5354551BC18373 because a test",
	});
	expect_refused(f, table_text(f, large), 11, "a computed correction of a published row its command does not recompute");
	large[7] = "# from published GIMPSReportLL retrieved 2026-10-18: a later report";
	large[8] = "ll 1000193 0123456789ABCDEF agreed=3 supersedes=8F5354551BC18373 because a test";
	known::Table republished = must_read(f, table_text(f, large));
	check::require(republished.rows.size() == 5, "five final residues");
	check::expect(republished.rows[3].superseded and not republished.rows[4].superseded, "a published correction of a published row");
}


static void the_loader_refuses_a_fingerprint_with_fewer_than_two_engines() {
	//
	// A fingerprint names two or more distinct engines and corrects nothing, and runs belong to
	// fingerprints only

	const std::string p = "partial-residues";
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0")), 7, "one run");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=fp64:72:5")), 7,
	               "two runs of one engine");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 4, "ll 1277 4 100 EE3E8EF89BAFBDCD")), 7, "a fingerprint with no runs");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 2, "prp 1277 3 100 C6D7DFE3FCAEDC31 run=fp64:64:0 run=ntt:72:5")), 5,
	               "runs on a computed row");
	expect_refused(p, table_text(p, with(PARTIAL_BODY, 5, "ll 1277 4 100 0123456789ABCDEF run=fp64:64:0 run=ntt:72:5 "
	                                                      "supersedes=EE3E8EF89BAFBDCD because a run")), 8,
	               "a fingerprint that supersedes");
}


static void every_known_answer_table_loads() {
	//
	// Each table under tests/known/ is read without a refusal, whose message names the table and
	// the row

	check::expect(known::TABLES == std::vector<std::string>({EXPONENTS, FINAL, PARTIAL}), "the tables, in order");
	for (const std::string& path: known::TABLES) {
		try {
			check::expect(not known::load(path).rows.empty(), path + " to hold rows");
		} catch (known_error const& error) {
			check::record(false, path + " to load", std::source_location::current(), error.what());
		}
	}
}


static void every_published_source_has_a_bibliography_entry() {
	//
	// The key of each published source is an entry of doc/references.md

	std::ifstream in("doc/references.md");
	check::require(in.good(), "doc/references.md to open");
	std::string bibliography((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::size_t published = 0;
	for (const std::string& path: known::TABLES)
		for (const known::Source& source: must_load(path).sources)
			if (source.origin == known::Origin::PUBLISHED) {
				published++;
				check::expect(bibliography.find("\n### " + source.reference + "\n") != std::string::npos,
				              path + ":" + std::to_string(source.line) + " to cite an entry of doc/references.md, "
				              + source.reference);
			}
	check::expect(published > 0, "a published source");
}


static void exponents_above_10000_cite_a_published_list() {
	//
	// The exponents up to the bound are computed, by the table's command; those above it cite the
	// published list, which the command reproduces without recomputing

	known::Table exponents = must_load(EXPONENTS);
	std::size_t above = 0;
	for (const known::Row& row: exponents.rows) {
		bool cites = exponents.sources[row.source].origin == known::Origin::PUBLISHED;
		bool computed = exponents.sources[row.source].origin == known::Origin::COMPUTED;
		if (row.exponent > RECOMPUTED_UP_TO)
			above++;
		check::expect(row.exponent > RECOMPUTED_UP_TO ? cites : computed,
		              EXPONENTS + ":" + std::to_string(row.line) + " to be "
		              + (row.exponent > RECOMPUTED_UP_TO ? "published" : "computed"));
	}
	check::expect(above > 0, "an exponent above the bound");
}


static void final_residues_agree_with_the_reference_residues() {
	//
	// Each final residue in force (a correction, never the row it replaced) whose test and exponent
	// tests/vectors/reference.txt holds is the low 64 bits of that full residue

	std::size_t compared = 0;
	for (const known::Row& row: must_load(FINAL).rows) {
		if (row.superseded)
			continue;
		for (const vectors::Record* record: vectors::records("reference", row.op == "ll" ? "ll" : "prp")) {
			if (vectors::number(record->fields[1]) != row.exponent)
				continue;
			compared++;
			check::expect_equal(row.res64, hex64(res64(vectors::operand(record->fields[2]), row.exponent)),
			                    FINAL + ":" + std::to_string(row.line) + " to be the low 64 bits of " + record->where());
		}
	}
	check::expect(compared >= 2, "two or more final residues compared with the reference residues");
}


static void final_residues_are_of_composite_mersenne_numbers() {
	//
	// No final residue is of an exponent the exponent table lists

	std::set<std::uint64_t> primes;
	for (const known::Row& row: must_load(EXPONENTS).rows)
		primes.insert(row.exponent);
	for (const known::Row& row: must_load(FINAL).rows)
		check::expect(primes.count(row.exponent) == 0,
		              FINAL + ":" + std::to_string(row.line) + " to be of an exponent with M_p composite, which "
		              + EXPONENTS + " does not list");
}


static check::Suite
check_by_name {
	{"the loader reads a table it is given",                          the_loader_reads_a_table_it_is_given},
	{"the loader refuses a malformed table",                          the_loader_refuses_a_malformed_table},
	{"the loader refuses a row without a source",                     the_loader_refuses_a_row_without_a_source},
	{"the loader refuses two answers for one key",                    the_loader_refuses_two_answers_for_one_key},
	{"the loader honours supersession",                               the_loader_honours_supersession},
	{"the loader refuses a fingerprint with fewer than two engines",  the_loader_refuses_a_fingerprint_with_fewer_than_two_engines},
	{"every known-answer table loads",                                every_known_answer_table_loads},
	{"every published source has a bibliography entry",               every_published_source_has_a_bibliography_entry},
	{"exponents above 10000 cite a published list",                   exponents_above_10000_cite_a_published_list},
	{"final residues agree with the reference residues",              final_residues_agree_with_the_reference_residues},
	{"final residues are of composite Mersenne numbers",              final_residues_are_of_composite_mersenne_numbers},
};
