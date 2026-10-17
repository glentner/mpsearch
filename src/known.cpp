// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Known answers
//
// The format, as tests/known/generate.py writes it: LF line ends, printable ASCII, fields
// separated by single spaces. The magic line; the command that writes the table; notes; then
// source lines and rows; and last the trailer, `# end <count> rows`. A source line,
// `# from published <key> retrieved <date>: …`, `# from computed: …` or `# from runs: …`, is the
// source of every row below it up to the next one.
//
// The loader refuses a table at the first rule it breaks, in a fixed order: the line rules top to
// bottom (the envelope first, then each source line and row), then the trailer's count, the order
// of the exponents, the corrections and last the conflicts. So a table with one defect is refused
// at the defect's line.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

// Internal libs
#include "known.hpp"


static const std::string MAGIC = "# mps known 1";

static const std::string EXPONENT_STEM = "mersenne-exponents";
static const std::string FINAL_STEM = "final-residues";
static const std::string PARTIAL_STEM = "partial-residues";

// Below this exponent the table's command, tests/known/generate.py, recomputes a published final
// residue, so a computed row may correct one; at or above it only another published row may. The
// command holds the same bound, and a change to one changes the other.
static const std::uint64_t RECOMPUTED_BELOW = 200'000;


// The rows of one table: the ops they may name, and the fields after the op, in order, each a
// letter: the exponent (P), the start (S), the iteration count (K) and the Res64 (H)
struct Shape {
	std::set<std::string> ops;
	std::string fields;
};


static std::map<std::string, Shape>
shape_by_stem {
	{EXPONENT_STEM,  {{"mersenne"},    "P"}},
	{FINAL_STEM,     {{"ll", "prp1"},  "PH"}},
	{PARTIAL_STEM,   {{"ll", "prp"},   "PSKH"}},
};


namespace known {


	const std::vector<std::string> TABLES = {
		"tests/known/mersenne-exponents.txt",
		"tests/known/final-residues.txt",
		"tests/known/partial-residues.txt",
	};

}


[[noreturn]] static void refuse(const std::string& path, std::size_t line, const std::string& reason) {
	throw known_error(path + ":" + std::to_string(line) + ": " + reason);
}


static std::vector<std::string> split(const std::string& text, char separator) {
	//
	// The parts of text between separators, empty ones included

	std::vector<std::string> parts;
	std::size_t start = 0;
	while (true) {
		std::size_t end = text.find(separator, start);
		parts.push_back(text.substr(start, end - start));
		if (end == std::string::npos)
			return parts;
		start = end + 1;
	}
}


static bool is_digit(char c) {
	return c >= '0' and c <= '9';
}


static bool is_lower(char c) {
	return c >= 'a' and c <= 'z';
}


static bool is_upper(char c) {
	return c >= 'A' and c <= 'Z';
}


static bool is_printable(const std::string& text) {
	//
	// Printable ASCII only, 0x20 to 0x7E

	for (char c: text)
		if (c < 0x20 or c > 0x7e)
			return false;
	return true;
}


static bool is_numeral(const std::string& text) {
	//
	// One to twenty decimal digits with no leading zero

	if (text.empty() or text.size() > 20 or (text.size() > 1 and text[0] == '0'))
		return false;
	for (char c: text)
		if (not is_digit(c))
			return false;
	return true;
}


static bool is_decimal(const std::string& text) {
	//
	// A numeral of at most 2^64 - 1, so std::stoull reads it

	return is_numeral(text) and (text.size() < 20 or text <= "18446744073709551615");
}


static bool is_res64(const std::string& text) {
	//
	// Sixteen uppercase hexadecimal digits, the form hex64 prints

	if (text.size() != 16)
		return false;
	for (char c: text)
		if (not (is_digit(c) or (c >= 'A' and c <= 'F')))
			return false;
	return true;
}


static bool is_prime(std::uint64_t n) {
	//
	// Trial division, for exponents below 2^32

	if (n < 2)
		return false;
	for (std::uint64_t d = 2; d * d <= n; d++)
		if (n % d == 0)
			return false;
	return true;
}


static bool is_key(const std::string& text) {
	//
	// A bibliography key: a letter, then letters, digits and hyphens

	if (text.empty() or not (is_upper(text[0]) or is_lower(text[0])))
		return false;
	for (char c: text)
		if (not (is_upper(c) or is_lower(c) or is_digit(c) or c == '-'))
			return false;
	return true;
}


static bool is_date(const std::string& text) {
	//
	// YYYY-MM-DD, the month 01 to 12 and the day 01 to 31

	if (text.size() != 10 or text[4] != '-' or text[7] != '-')
		return false;
	for (std::size_t i: {0, 1, 2, 3, 5, 6, 8, 9})
		if (not is_digit(text[i]))
			return false;
	int month = 10 * (text[5] - '0') + (text[6] - '0');
	int day = 10 * (text[8] - '0') + (text[9] - '0');
	return month >= 1 and month <= 12 and day >= 1 and day <= 31;
}


static bool is_engine(const std::string& text) {
	//
	// An engine's name: lowercase letters, digits and hyphens

	if (text.empty())
		return false;
	for (char c: text)
		if (not (is_lower(c) or is_digit(c) or c == '-'))
			return false;
	return true;
}


static std::string command_of(const std::string& stem) {
	return "# command: python3 tests/known/generate.py " + stem + " > tests/known/" + stem + ".txt";
}


static std::string key_of(const known::Row& row) {
	//
	// What determines a row's answer: the test and the exponent, and for a partial residue (the
	// only rows with an iteration count) the start and the iteration count

	std::string key = row.op + " " + std::to_string(row.exponent);
	if (row.iterations != 0)
		key += " " + std::to_string(row.start) + " " + std::to_string(row.iterations);
	return key;
}


static known::Source read_source(const std::string& path, std::size_t line, const std::string& text) {
	//
	// A source line, `# from published <key> retrieved <date>: <text>`, `# from computed: <text>` or
	// `# from runs: <text>`

	std::size_t colon = text.find(": ");
	if (colon == std::string::npos or colon + 2 >= text.size())
		refuse(path, line, "a source line names its source, then ': ' and what it says");
	std::vector<std::string> head = split(text.substr(7, colon - 7), ' ');
	known::Source source {known::Origin::COMPUTED, "", "", text.substr(colon + 2), line};
	if (head.size() == 1 and head[0] == "computed")
		return source;
	if (head.size() == 1 and head[0] == "runs") {
		source.origin = known::Origin::RUNS;
		return source;
	}
	if (head.size() == 4 and head[0] == "published" and head[2] == "retrieved") {
		if (not is_key(head[1]))
			refuse(path, line, "'" + head[1] + "' is not a bibliography key");
		if (not is_date(head[3]))
			refuse(path, line, "'" + head[3] + "' is not a date, YYYY-MM-DD");
		source.origin = known::Origin::PUBLISHED;
		source.reference = head[1];
		source.retrieved = head[3];
		return source;
	}
	refuse(path, line, "a source is 'published <key> retrieved <date>', 'computed' or 'runs'");
}


static known::Row read_row(const std::string& path, std::size_t line, const std::string& text,
                           const std::string& stem, const known::Source& source) {
	//
	// One row: its op, its fields in the table's order, then its attributes, each allowed only
	// where its rule says: agreed=<n> (published final residues), run=<engine>:<length>:<shift>
	// (fingerprints) and, last, supersedes=<Res64> because <reason>

	std::string head = text, reason;
	std::size_t because = text.find(" because ");
	if (because != std::string::npos) {
		head = text.substr(0, because);
		reason = text.substr(because + 9);
		if (reason.empty() or reason[0] == ' ')
			refuse(path, line, "'because' is followed by the reason");
	}
	std::vector<std::string> tokens = split(head, ' ');
	for (const std::string& token: tokens)
		if (token.empty())
			refuse(path, line, "fields are separated by single spaces");

	known::Row row;
	row.line = line;
	row.op = tokens[0];
	const Shape& shape = shape_by_stem.at(stem);
	if (shape.ops.count(row.op) == 0)
		refuse(path, line, "'" + row.op + "' is not an op of " + stem);
	if (tokens.size() < shape.fields.size() + 1)
		refuse(path, line, row.op + " takes " + std::to_string(shape.fields.size()) + " fields");
	for (std::size_t k = 0; k < shape.fields.size(); k++) {
		const std::string& field = tokens[k + 1];
		char kind = shape.fields[k];
		if (kind == 'H') {
			if (not is_res64(field))
				refuse(path, line, "'" + field + "' is not a Res64, 16 uppercase hexadecimal digits");
			row.res64 = field;
			continue;
		}
		if (not is_decimal(field))
			refuse(path, line, "'" + field + "' is not a decimal number");
		std::uint64_t value = std::stoull(field);
		if (kind == 'P') {
			if (value >= (std::uint64_t(1) << 32) or not is_prime(value) or (stem != EXPONENT_STEM and value < 3))
				refuse(path, line, "the exponent " + field + " is not a prime of this table's range");
			row.exponent = value;
		} else if (kind == 'S') {
			row.start = value;
		} else {
			if (value == 0)
				refuse(path, line, "the iteration count is at least 1");
			row.iterations = value;
		}
	}

	bool has_agreed = false;
	for (std::size_t k = shape.fields.size() + 1; k < tokens.size(); k++) {
		const std::string& token = tokens[k];
		std::size_t equals = token.find('=');
		std::string name = token.substr(0, equals);
		std::string value = equals == std::string::npos ? "" : token.substr(equals + 1);
		if (not row.supersedes.empty())
			refuse(path, line, "supersedes= is the last attribute");
		if (name == "agreed" and stem == FINAL_STEM and not has_agreed) {
			if (not is_decimal(value) or std::stoull(value) < 2)
				refuse(path, line, "agreed= counts two or more agreeing results");
			row.agreed = std::stoull(value);
			has_agreed = true;
		} else if (name == "run" and stem == PARTIAL_STEM) {
			std::vector<std::string> parts = split(value, ':');
			if (parts.size() != 3 or not is_engine(parts[0]) or not is_decimal(parts[1]) or not is_decimal(parts[2])
			    or std::stoull(parts[1]) == 0 or std::stoull(parts[2]) >= row.exponent)
				refuse(path, line, "'" + token + "' is not run=<engine>:<length>:<shift>, the shift below p");
			row.runs.push_back({parts[0], std::stoull(parts[1]), std::stoull(parts[2])});
		} else if (name == "supersedes" and stem != EXPONENT_STEM) {
			if (not is_res64(value))
				refuse(path, line, "supersedes= names the Res64 of the row it replaces");
			row.supersedes = value;
		} else {
			refuse(path, line, "'" + token + "' is not an attribute of a row of " + stem);
		}
	}
	if (row.supersedes.empty() != reason.empty())
		refuse(path, line, "a row that supersedes another says why, 'because <reason>', and only such a row does");
	row.reason = reason;

	bool published_final = stem == FINAL_STEM and source.origin == known::Origin::PUBLISHED;
	if (published_final != has_agreed)
		refuse(path, line, "agreed= is on every published final residue and on no other row");
	if (source.origin == known::Origin::RUNS) {
		std::set<std::string> engines;
		for (const known::Run& run: row.runs)
			engines.insert(run.engine);
		if (engines.size() < 2)
			refuse(path, line, "a fingerprint names two or more distinct engines");
		if (not row.supersedes.empty())
			refuse(path, line, "a fingerprint supersedes no row");
	} else if (not row.runs.empty()) {
		refuse(path, line, "run= belongs to a fingerprint, a row under '# from runs:'");
	}
	if (stem == PARTIAL_STEM and row.op == "ll" and row.start == 4 and row.iterations == row.exponent - 2)
		refuse(path, line, "s_(p-2) from 4 is the final Lucas-Lehmer residue, which belongs in " + FINAL_STEM);
	return row;
}


static void apply_corrections(known::Table& table) {
	//
	// Mark the row each correction replaces
	//
	// A correction names, by its Res64, the earliest row above it of its key that no other row has
	// replaced. It is never weaker than that row: in the final table a computed row cannot replace
	// a published one the command does not recompute. (A fingerprint supersedes nothing, which
	// read_row refuses.)

	for (known::Row& row: table.rows) {
		if (row.supersedes.empty())
			continue;
		known::Row* replaced = nullptr;
		for (known::Row& earlier: table.rows) {
			if (&earlier == &row)
				break;
			if (not earlier.superseded and key_of(earlier) == key_of(row) and earlier.res64 == row.supersedes) {
				replaced = &earlier;
				break;
			}
		}
		if (replaced == nullptr)
			refuse(table.path, row.line, "no row of " + key_of(row) + " above this one gives " + row.supersedes);
		bool computed = table.sources[row.source].origin == known::Origin::COMPUTED;
		bool published = table.sources[replaced->source].origin == known::Origin::PUBLISHED;
		if (table.stem == FINAL_STEM and computed and published and replaced->exponent >= RECOMPUTED_BELOW)
			refuse(table.path, row.line, "a computed row cannot replace a published one its command does not recompute");
		replaced->superseded = true;
	}
}


static void refuse_conflicts(const known::Table& table) {
	//
	// Refuse two rows of one key with different answers, neither replaced, at the later one

	std::map<std::string, const known::Row*> first_by_key;
	for (const known::Row& row: table.rows) {
		if (row.superseded or row.res64.empty())
			continue;
		auto [first, added] = first_by_key.emplace(key_of(row), &row);
		if (not added and first->second->res64 != row.res64)
			refuse(table.path, row.line, key_of(row) + " is " + row.res64 + " here and " + first->second->res64
			                             + " at line " + std::to_string(first->second->line) + ", and neither row is replaced");
	}
}


known::Table known::parse(const std::string& path, const std::string& text) {
	//
	// Read one table from its text, refusing it at the first rule it breaks
	//
	// The order of the rules is the format's (see the top of this file); within the line rules, the
	// first line that breaks one, top to bottom.

	// The parts between LFs; the last, empty after a final LF, is no line
	std::vector<std::string> lines = split(text, '\n');
	if (text.empty() or text.back() != '\n')
		refuse(path, lines.size(), "the table ends in LF");
	lines.pop_back();
	for (std::size_t i = 0; i < lines.size(); i++)
		if (lines[i].empty() or not is_printable(lines[i]))
			refuse(path, i + 1, "a line is nonempty printable ASCII");
	if (lines[0] != MAGIC)
		refuse(path, 1, "the table opens with '" + MAGIC + "'");
	if (lines.size() < 3)
		refuse(path, lines.size(), "the table holds its magic line, its command and its trailer");

	known::Table table;
	table.path = path;
	for (const auto& [stem, shape]: shape_by_stem)
		if (lines[1] == command_of(stem))
			table.stem = stem;
	// The basename of path: all of it when it holds no '/', as npos + 1 is 0
	std::string file = path.substr(path.rfind('/') + 1);
	if (table.stem.empty() or file != table.stem + ".txt")
		refuse(path, 2, "the second line is the command that writes this table");
	std::vector<std::string> trailer = split(lines.back(), ' ');
	if (trailer.size() != 4 or trailer[0] != "#" or trailer[1] != "end" or not is_numeral(trailer[2]) or trailer[3] != "rows")
		refuse(path, lines.size(), "the last line is the trailer, '# end <count> rows'");

	for (std::size_t i = 2; i + 1 < lines.size(); i++) {
		const std::string& line = lines[i];
		if (line.rfind("# from ", 0) == 0) {
			table.sources.push_back(read_source(path, i + 1, line));
		} else if (line.rfind("# end ", 0) == 0) {
			refuse(path, i + 1, "the trailer is the last line");
		} else if (line.rfind("# ", 0) == 0) {
			if (not table.sources.empty())
				refuse(path, i + 1, "notes come before the first source line");
			table.notes.push_back(line.substr(2));
		} else if (line[0] == '#') {
			refuse(path, i + 1, "a comment line starts with '# '");
		} else {
			if (table.sources.empty())
				refuse(path, i + 1, "a row has a source line above it");
			known::Row row = read_row(path, i + 1, line, table.stem, table.sources.back());
			row.source = table.sources.size() - 1;
			table.rows.push_back(row);
		}
	}
	if (trailer[2] != std::to_string(table.rows.size()))
		refuse(path, lines.size(), "the trailer counts " + trailer[2] + " rows, and the table holds "
		                           + std::to_string(table.rows.size()));
	if (table.stem == EXPONENT_STEM)
		for (std::size_t i = 1; i < table.rows.size(); i++)
			if (table.rows[i].exponent <= table.rows[i - 1].exponent)
				refuse(path, table.rows[i].line, "the exponents ascend strictly");
	apply_corrections(table);
	refuse_conflicts(table);
	return table;
}


known::Table known::load(const std::string& path) {
	//
	// Read one table from its file, a path from the working directory

	std::ifstream in(path, std::ios::binary);
	if (not in.good())
		throw known_error(path + ": does not open");
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return parse(path, text);
}
