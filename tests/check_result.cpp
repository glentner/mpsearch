// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: results
//
// Each outcome is built directly, since no test decides anything yet, and run through the
// verdict line, the JSON record, the exit status and the printer, in both of its modes. The
// statuses are compared as literal numbers, never as `Status` names, which an edit could move.
// The residues here are synthetic and never paired with a real composite exponent. Every
// record must pass a strict JSON reader that lives only here, and that reader is itself
// checked to reject each kind of bad text.
//


// Standard libs
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "log.hpp"
#include "log_peer.hpp"
#include "main.hpp"
#include "math.hpp"
#include "result.hpp"


namespace {


	class JsonReader {
	public:

		//
		// A strict reader for one JSON object (RFC 8259, 2017)
		//
		// Accepts objects, arrays, strings and their escapes, integers, and `true`, `false`
		// and `null`. Rejects duplicate keys, trailing commas, leading zeros, fractions and
		// exponents, raw control characters, unknown escapes, any other top level, and
		// trailing text. Keeps the top-level keys in order and their string values decoded.

		JsonReader(const std::string& text): text(text) {}

		std::vector<std::string> keys {};
		std::map<std::string, std::string> string_by_key {};

		bool valid() {
			at = 0;
			depth = 0;
			keys.clear();
			string_by_key.clear();
			space();
			if (not peek('{') or not object())
				return false;
			space();
			return at == text.size();
		}

	private:

		const std::string text;
		size_t at = 0;
		int depth = 0;

		bool peek(char c) const {
			return at < text.size() and text[at] == c;
		}

		bool take(char c) {
			if (not peek(c))
				return false;
			at++;
			return true;
		}

		void space() {
			while (peek(' ') or peek('\t') or peek('\n') or peek('\r'))
				at++;
		}

		bool literal(const std::string& word) {
			if (text.compare(at, word.size(), word) != 0)
				return false;
			at += word.size();
			return true;
		}

		bool number() {
			take('-');
			if (take('0')) {
				if (at < text.size() and text[at] >= '0' and text[at] <= '9')
					return false;
			} else {
				if (not (at < text.size() and text[at] >= '1' and text[at] <= '9'))
					return false;
				while (at < text.size() and text[at] >= '0' and text[at] <= '9')
					at++;
			}
			return not (peek('.') or peek('e') or peek('E'));
		}

		bool string(std::string& decoded) {
			static const std::map<char, char> byte_by_escape {
				{'"',  '"'},
				{'\\', '\\'},
				{'/',  '/'},
				{'b',  '\b'},
				{'f',  '\f'},
				{'n',  '\n'},
				{'r',  '\r'},
				{'t',  '\t'},
			};
			if (not take('"'))
				return false;
			while (at < text.size()) {
				const unsigned char c = text[at++];
				if (c == '"')
					return true;
				if (c < 0x20)
					return false;
				if (c != '\\') {
					decoded += char(c);
					continue;
				}
				if (at >= text.size())
					return false;
				const char e = text[at++];
				if (byte_by_escape.count(e)) {
					decoded += byte_by_escape.at(e);
				} else if (e == 'u') {
					if (at + 4 > text.size())
						return false;
					unsigned code = 0;
					for (int i = 0; i < 4; i++) {
						const char h = text[at++];
						code <<= 4;
						if (h >= '0' and h <= '9')
							code += h - '0';
						else if (h >= 'a' and h <= 'f')
							code += h - 'a' + 10;
						else if (h >= 'A' and h <= 'F')
							code += h - 'A' + 10;
						else
							return false;
					}
					decoded += code < 0x80 ? char(code) : '?';
				} else {
					return false;
				}
			}
			return false;
		}

		bool array() {
			take('[');
			space();
			if (take(']'))
				return true;
			while (true) {
				if (not value())
					return false;
				space();
				if (take(']'))
					return true;
				if (not take(','))
					return false;
				space();
			}
		}

		bool object() {
			take('{');
			depth++;
			std::set<std::string> seen;
			space();
			if (take('}')) {
				depth--;
				return true;
			}
			while (true) {
				std::string key;
				if (not string(key) or seen.count(key))
					return false;
				seen.insert(key);
				space();
				if (not take(':'))
					return false;
				space();
				if (depth == 1) {
					keys.push_back(key);
					if (peek('"')) {
						std::string decoded;
						if (not string(decoded))
							return false;
						string_by_key[key] = decoded;
					} else if (not value()) {
						return false;
					}
				} else if (not value()) {
					return false;
				}
				space();
				if (take('}')) {
					depth--;
					return true;
				}
				if (not take(','))
					return false;
				space();
			}
		}

		bool value() {
			std::string ignored;
			if (peek('{'))
				return object();
			if (peek('['))
				return array();
			if (peek('"'))
				return string(ignored);
			if (peek('t'))
				return literal("true");
			if (peek('f'))
				return literal("false");
			if (peek('n'))
				return literal("null");
			return number();
		}
	};

}


static bool valid_json(const std::string& text) {
	JsonReader reader(text);
	return reader.valid();
}


static bool printable_ascii(const std::string& text) {
	for (const unsigned char c: text)
		if (c < 0x20 or c > 0x7e)
			return false;
	return true;
}


static bool well_formed_utf8(const std::string& text) {
	//
	// Whether text is well-formed UTF-8 (RFC 3629, 2003)
	//
	// Decodes each sequence from its lead byte's high bits, then rejects a sequence cut short
	// or holding a byte that is not a continuation, an overlong form, a surrogate, and any code
	// point beyond U+10FFFF.

	size_t at = 0;
	while (at < text.size()) {
		const unsigned char lead = text[at];
		size_t length = 0;
		unsigned point = 0;
		if (lead < 0x80) {
			length = 1;
			point = lead;
		} else if ((lead & 0xe0) == 0xc0) {
			length = 2;
			point = lead & 0x1f;
		} else if ((lead & 0xf0) == 0xe0) {
			length = 3;
			point = lead & 0x0f;
		} else if ((lead & 0xf8) == 0xf0) {
			length = 4;
			point = lead & 0x07;
		} else {
			return false;
		}
		if (at + length > text.size())
			return false;
		for (size_t i = 1; i < length; i++) {
			const unsigned char c = text[at + i];
			if ((c & 0xc0) != 0x80)
				return false;
			point = (point << 6) | (c & 0x3f);
		}
		static const unsigned smallest_by_length[] = {0, 0, 0x80, 0x800, 0x1'0000};
		if (point < smallest_by_length[length] or (point >= 0xd800 and point <= 0xdfff) or point > 0x10'ffff)
			return false;
		at += length;
	}
	return true;
}


static std::string tail() {
	//
	// The end of every verdict line: the program and its version

	return " (mps " + APP_VERSION + ")";
}


static std::string program() {
	//
	// The end of every record: the program, its version and the record's own version

	return "\"program\": {\"name\": \"mps\", \"version\": \"" + APP_VERSION + "\"}, \"record_version\": 1}";
}


// One constructed result of each kind, with the line and record it must give and its status
struct Shown {
	std::string kind;
	Result result;
	std::string line;
	std::string record;
	int status;
};


static std::vector<Shown> each_outcome() {
	return {
		{"no verdict", Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet"),
		 "M31 tf: no verdict, trial factoring is not implemented yet" + tail(),
		 "{\"exponent\": 31, \"worktype\": \"tf\", \"status\": \"no-verdict\", \"reason\": \"trial factoring is not implemented yet\", " + program(),
		 42},
		{"prime", Result::prime(WORK_LL, 31),
		 "M31 ll: prime" + tail(),
		 "{\"exponent\": 31, \"worktype\": \"ll\", \"status\": \"prime\", " + program(),
		 0},
		{"probable prime", Result::probable_prime(WORK_LL, 31),
		 "M31 ll: probable prime" + tail(),
		 "{\"exponent\": 31, \"worktype\": \"ll\", \"status\": \"probable-prime\", " + program(),
		 41},
		{"composite by factor", Result::composite_by_factor(WORK_PTEST, 33, "2^3-1"),
		 "M33 ptest: composite, factor 2^3-1" + tail(),
		 "{\"exponent\": 33, \"worktype\": \"ptest\", \"status\": \"composite\", \"factors\": [\"2^3-1\"], " + program(),
		 40},
		{"composite by residue", Result::composite_by_residue(WORK_LL, 999'999'937, 0x0123'4567'89AB'CDEF),
		 "M999999937 ll: composite, res64 0123456789ABCDEF" + tail(),
		 "{\"exponent\": 999999937, \"worktype\": \"ll\", \"status\": \"composite\", \"res64\": \"0123456789ABCDEF\", " + program(),
		 40},
		{"no verdict from P-1", Result::no_verdict(WORK_P1, 7, "P-1 is not implemented yet"),
		 "M7 p1: no verdict, P-1 is not implemented yet" + tail(),
		 "{\"exponent\": 7, \"worktype\": \"p1\", \"status\": \"no-verdict\", \"reason\": \"P-1 is not implemented yet\", " + program(),
		 42},
	};
}


static Result an_error() {
	return Result::error(WORK_LL, 31, "a constructed failure");
}


// What the printer did with one run
struct Printed {
	std::string out;
	std::string err;
	int status;
};


static Printed print_run(const std::vector<Result>& results, bool json) {
	//
	// The printer over a run, as `main` drives it
	//
	// `report_result` for each result in turn, then `report_run`, with stderr captured.

	std::ostringstream out, err;
	std::streambuf* saved = std::cerr.rdbuf(err.rdbuf());
	std::vector<Result> run;
	for (const Result& result: results)
		report_result(run, result, json, out);
	const int status = report_run(run, json, out);
	std::cerr.rdbuf(saved);
	return {out.str(), err.str(), status};
}


static std::string shown(const Result& result, bool json) {
	return (json ? json_record(result) : verdict_line(result)) + "\n";
}


static std::string mode(bool json) {
	return json ? " (json)" : " (plain)";
}


static void each_outcome_has_its_line() {
	//
	// Each outcome's verdict line, exactly
	//
	// The exponent, the worktype, the outcome, the factor, residue or reason, and the
	// program with its version.

	for (const Shown& shown: each_outcome())
		check::expect_equal(verdict_line(shown.result), shown.line, "the line of a " + shown.kind);
}


static void each_outcome_has_its_record() {
	//
	// Each outcome's JSON record, exactly
	//
	// Keys in order, spaced as Python's `json.dumps` writes them, with `program` and
	// `record_version` on every one.

	for (const Shown& shown: each_outcome())
		check::expect_equal(json_record(shown.result), shown.record, "the record of a " + shown.kind);
	check::expect_equal(RECORD_VERSION, 1, "the record's format version is 1");
}


static void an_error_has_a_line_and_no_record() {
	//
	// An error's line, which only stderr shows, and no record

	check::expect_equal(verdict_line(an_error()), "M31 ll: error, a constructed failure" + tail(), "the line of an error");
	check::expect_equal(json_record(an_error()), std::string(""), "no record for an error");
}


static void residues_are_sixteen_uppercase_digits() {
	//
	// A residue is zero-padded to 16 digits, in uppercase

	check::expect_equal(verdict_line(Result::composite_by_residue(WORK_LL, 31, 0x1)),
	                    "M31 ll: composite, res64 0000000000000001" + tail(), "a residue of 1, padded");
	check::expect_equal(json_record(Result::composite_by_residue(WORK_LL, 31, 0xABCD'EF01'2345'6789)),
	                    "{\"exponent\": 31, \"worktype\": \"ll\", \"status\": \"composite\", \"res64\": \"ABCDEF0123456789\", " + program(),
	                    "a residue in uppercase");
}


static void every_record_is_valid_json() {
	//
	// Every record is one strictly valid JSON object of printable ASCII
	//
	// With its keys in the record's order.

	for (const Shown& shown: each_outcome()) {
		const std::string record = json_record(shown.result);
		check::expect(valid_json(record), "the record of a " + shown.kind + " is valid JSON");
		check::expect(printable_ascii(record), "the record of a " + shown.kind + " is printable ASCII");

		JsonReader reader(record);
		reader.valid();
		check::expect_equal(reader.keys.front(), std::string("exponent"), "the record of a " + shown.kind + " starts with exponent");
		check::expect_equal(reader.keys.back(), std::string("record_version"), "the record of a " + shown.kind + " ends with record_version");
	}
}


static void the_json_reader_rejects_bad_text() {
	//
	// The reader itself rejects each kind of bad text, and accepts good text
	//
	// A reader that accepted everything would make every record pass.

	static const std::vector<std::string> bad = {
		"",
		"[1]",
		"\"text\"",
		"{",
		"{\"a\": 1,}",
		"{\"a\": [1, 2,]}",
		"{\"a\": 1, \"a\": 2}",
		"{\"a\": {\"b\": 1, \"b\": 2}}",
		"{\"a\": 01}",
		"{\"a\": -01}",
		"{\"a\": 1.5}",
		"{\"a\": 1e5}",
		"{\"a\": NaN}",
		"{\"a\": tru}",
		"{\"a\" 1}",
		"{a: 1}",
		"{\"a\": \"x\ny\"}",
		"{\"a\": \"x\x01y\"}",
		"{\"a\": \"\\q\"}",
		"{\"a\": \"\\u00g1\"}",
		"{\"a\": \"open}",
		"{\"a\": 1} x",
		"{\"a\": 1}{}",
	};
	for (const std::string& text: bad)
		check::expect(not valid_json(text), "the reader rejects '" + text + "'");

	static const std::vector<std::string> good = {
		"{}",
		" {\"a\": 0} ",
		"{\"a\": [], \"b\": {\"c\": null}, \"d\": [true, false, -3, 0, 120], \"e\": \"\\u0041\\\"\\\\\\/\\b\\f\\n\\r\\t\"}",
	};
	for (const std::string& text: good)
		check::expect(valid_json(text), "the reader accepts '" + text + "'");

	JsonReader reader("{\"a\": \"x\\u0041\\n\", \"b\": 1}");
	check::require(reader.valid(), "the reader accepts a record to decode");
	check::expect_equal(reader.string_by_key["a"], std::string("xA\n"), "the reader decodes a string's escapes");
}


static void a_hostile_reason_stays_one_record() {
	//
	// A reason holding a quote, a backslash and control characters gives one valid line
	//
	// Compared by value, since an escape may be spelled two ways.

	const std::string reason = "a\"b\\c\nd\x01" "e\x1f";
	const std::string record = json_record(Result::no_verdict(WORK_P1, 5, reason));

	check::expect(valid_json(record), "the hostile record is valid JSON");
	check::expect(printable_ascii(record), "the hostile record is one line of printable ASCII");

	JsonReader reader(record);
	check::require(reader.valid(), "the hostile record reads");
	check::expect_equal(reader.string_by_key["reason"], reason, "the hostile reason reads back unchanged");
	check::expect_equal(reader.string_by_key["status"], std::string("no-verdict"), "the hostile record's status");
}


static void a_hostile_reason_stays_one_line() {
	//
	// A reason holding control bytes gives one line, with each one escaped
	//
	// A newline in a reason must never start a second line that reads as a verdict, and an
	// escape must never reach the terminal. Both the no verdict's line and the error's line,
	// which stderr shows, are checked.

	struct Hostile {
		std::string reason;
		std::string shown;
	};
	const std::vector<Hostile> hostiles = {
		{"line one\nM31 ll: prime" + tail(), "line one\\x0aM31 ll: prime" + tail()},
		{"carriage\rreturn",                 "carriage\\x0dreturn"},
		{"tab\there",                        "tab\\x09here"},
		{"escape \x1b[31mred\x1b[0m",        "escape \\x1b[31mred\\x1b[0m"},
		{std::string("nul\0inside", 10),     "nul\\x00inside"},
		{"delete\x7f",                       "delete\\x7f"},
		{"unit\x1f" "separator",             "unit\\x1fseparator"},
	};
	for (const Hostile& hostile: hostiles) {
		const std::string line = verdict_line(Result::no_verdict(WORK_TF, 31, hostile.reason));
		const std::string error = verdict_line(Result::error(WORK_LL, 31, hostile.reason));
		check::expect_equal(line, "M31 tf: no verdict, " + hostile.shown + tail(), "the line for the reason '" + hostile.shown + "'");
		check::expect_equal(error, "M31 ll: error, " + hostile.shown + tail(), "the error's line for the reason '" + hostile.shown + "'");
		check::expect(printable_ascii(line), "the line for the reason '" + hostile.shown + "' holds no control byte");
		check::expect(printable_ascii(error), "the error's line for the reason '" + hostile.shown + "' holds no control byte");
	}
}


static void a_control_byte_reason_reads_back_from_its_record() {
	//
	// A reason holding NUL, a carriage return, ESC or DEL reads back unchanged from its record

	for (const std::string& reason: {std::string("nul\0inside", 10), std::string("carriage\rreturn"),
	                                 std::string("escape \x1b[31mred"), std::string("delete\x7f")}) {
		const std::string record = json_record(Result::no_verdict(WORK_TF, 31, reason));
		JsonReader reader(record);
		check::require(reader.valid(), "the record for a control-byte reason is valid JSON");
		check::expect(record.find_first_of("\n\r") == std::string::npos, "the record for a control-byte reason is one line");
		check::expect_equal(reader.string_by_key["reason"], reason, "a control-byte reason reads back unchanged");
	}
}


static void well_formed_utf8_passes_through() {
	//
	// A reason in well-formed UTF-8 stays exactly as it is, on the line and in the record

	const std::string reason = "caf\xc3\xa9, \xe2\x82\xac and \xf0\x9d\x84\x9e";
	check::expect_equal(verdict_line(Result::no_verdict(WORK_TF, 31, reason)), "M31 tf: no verdict, " + reason + tail(),
	                    "the line holds a UTF-8 reason as it is");
	check::expect_equal(json_record(Result::no_verdict(WORK_TF, 31, reason)),
	                    "{\"exponent\": 31, \"worktype\": \"tf\", \"status\": \"no-verdict\", \"reason\": \"" + reason + "\", " + program(),
	                    "the record holds a UTF-8 reason as it is");
	check::expect(well_formed_utf8(reason), "the checks' own UTF-8 reader accepts well-formed text");
}


static void a_malformed_utf8_reason_becomes_replacement_characters() {
	//
	// Each byte of a reason that begins or continues no well-formed UTF-8 sequence becomes
	// U+FFFD in the record
	//
	// One of each kind: a byte that never appears in UTF-8, a lone continuation, a sequence cut
	// short by the end or by an ASCII byte, an overlong form, a surrogate, and a code point
	// beyond U+10FFFF. The record stays valid JSON in UTF-8; the line keeps the bytes as they
	// are, since it carries no encoding of its own.

	const std::string R = "\xef\xbf\xbd";
	struct Malformed {
		std::string kind;
		std::string reason;
		std::string shown;
	};
	const std::vector<Malformed> malformed = {
		{"a byte never in UTF-8",           "bad \xff byte",           "bad " + R + " byte"},
		{"a lone continuation",             "\x80 lone continuation",  R + " lone continuation"},
		{"a sequence cut short",            "truncated \xf0\x9d\x84",  "truncated " + R + R + R},
		{"a lead without its continuation", "bad continuation \xc3(",  "bad continuation " + R + "("},
		{"an overlong form",                "overlong \xc0\xaf",       "overlong " + R + R},
		{"a surrogate",                     "surrogate \xed\xa0\x80",  "surrogate " + R + R + R},
		{"a code point beyond U+10FFFF",    "beyond \xf4\x90\x80\x80", "beyond " + R + R + R + R},
	};
	for (const Malformed& m: malformed) {
		check::expect(not well_formed_utf8(m.reason), "the checks' own UTF-8 reader rejects " + m.kind);
		const std::string record = json_record(Result::no_verdict(WORK_TF, 31, m.reason));
		check::expect(well_formed_utf8(record), "the record for " + m.kind + " is well-formed UTF-8");
		JsonReader reader(record);
		check::require(reader.valid(), "the record for " + m.kind + " is valid JSON");
		check::expect_equal(reader.string_by_key["reason"], m.shown, "the record for " + m.kind + " holds U+FFFD for each bad byte");
		check::expect_equal(verdict_line(Result::no_verdict(WORK_TF, 31, m.reason)), "M31 tf: no verdict, " + m.reason + tail(),
		                    "the line for " + m.kind + " keeps its bytes");
	}
}


static void only_verdicts_are_decisive() {
	//
	// Prime, probable prime and composite decide; a no verdict and an error do not

	check::expect(is_decisive(Result::prime(WORK_LL, 31)), "a prime is decisive");
	check::expect(is_decisive(Result::probable_prime(WORK_LL, 31)), "a probable prime is decisive");
	check::expect(is_decisive(Result::composite_by_factor(WORK_PTEST, 33, "2^3-1")), "a composite by factor is decisive");
	check::expect(is_decisive(Result::composite_by_residue(WORK_LL, 999'999'937, 0x0123'4567'89AB'CDEF)), "a composite by residue is decisive");
	check::expect(not is_decisive(Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet")), "a no verdict is not decisive");
	check::expect(not is_decisive(an_error()), "an error is not decisive");
}


static void each_outcome_has_its_status() {
	//
	// The status of a run of one result of each outcome
	//
	// 0 prime, 41 probable prime, 40 composite, 42 no verdict, 20 an error.

	for (const Shown& shown: each_outcome())
		check::expect_equal(status_of({shown.result}), shown.status, "the status of a " + shown.kind);
	check::expect_equal(status_of({an_error()}), 20, "the status of an error");
	check::expect_equal(status_of({}), 42, "the status of a run with no result");
}


static void the_status_table_keeps_its_values() {
	//
	// Each name in `enum Status` holds the number the help's exit-status table gives it
	//
	// The error classes keep the values they had before the verdicts joined them; a script
	// reads the numbers, so no edit may move one.

	check::expect_equal(static_cast<int>(OK), 0, "OK is 0");
	check::expect_equal(static_cast<int>(ERR_ARG), 1, "ERR_ARG is 1");
	check::expect_equal(static_cast<int>(ERR_CFG), 2, "ERR_CFG is 2");
	check::expect_equal(static_cast<int>(ERR_FILE), 10, "ERR_FILE is 10");
	check::expect_equal(static_cast<int>(ERR_RUN), 20, "ERR_RUN is 20");
	check::expect_equal(static_cast<int>(ERR_UNKNOWN), 30, "ERR_UNKNOWN is 30");
	check::expect_equal(static_cast<int>(COMPOSITE), 40, "COMPOSITE is 40");
	check::expect_equal(static_cast<int>(PROBABLE_PRIME), 41, "PROBABLE_PRIME is 41");
	check::expect_equal(static_cast<int>(NO_VERDICT), 42, "NO_VERDICT is 42");
}


static void no_status_is_reserved_for_burn_in() {
	//
	// No run of results reaches the statuses from 50 to 59
	//
	// Every pair of outcomes, in both orders.

	std::vector<Result> results;
	for (const Shown& shown: each_outcome())
		results.push_back(shown.result);
	results.push_back(an_error());

	for (const Result& first: results)
		for (const Result& second: results) {
			const int status = status_of({first, second});
			check::expect(status < 50 or status > 59, verdict_line(first) + " then " + verdict_line(second) + " is not reserved");
		}
}


static void the_printer_shows_each_outcome() {
	//
	// One result of each outcome through the printer, in both modes
	//
	// Its line or record and a newline on stdout, and its status as the run's.

	for (const bool json: {false, true})
		for (const Shown& shown: each_outcome()) {
			const Printed printed = print_run({shown.result}, json);
			check::expect_equal(printed.out, (json ? shown.record : shown.line) + "\n", "the printed " + shown.kind + mode(json));
			check::expect_equal(printed.status, shown.status, "the printer's status for a " + shown.kind + mode(json));
		}
}


static void an_error_prints_no_verdict() {
	//
	// A run holding an error exits 20, prints nothing for it, and reports it on stderr
	//
	// A verdict from before or after the error never prints either; a no verdict already
	// printed stays.

	const Result nv = Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet");
	const Result composite = Result::composite_by_factor(WORK_PTEST, 33, "2^3-1");

	struct Run {
		std::string label;
		std::vector<Result> results;
		std::vector<Result> printed;
	};
	const std::vector<Run> runs = {
		{"an error",                {an_error()},             {}},
		{"a no verdict, an error",  {nv, an_error()},         {nv}},
		{"a composite, an error",   {composite, an_error()},  {}},
		{"an error, a composite",   {an_error(), composite},  {}},
	};

	for (const bool json: {false, true})
		for (const Run& run: runs) {
			const Printed printed = print_run(run.results, json);
			std::string expected;
			for (const Result& result: run.printed)
				expected += shown(result, json);
			check::expect_equal(printed.status, 20, run.label + ": the status" + mode(json));
			check::expect_equal(printed.out, expected, run.label + ": stdout" + mode(json));
			check::expect(printed.err.find("a constructed failure") != std::string::npos, run.label + ": the reason on stderr" + mode(json));
		}
}


static void a_contradiction_prints_no_verdict() {
	//
	// A prime or a probable prime with a composite exits 30 and prints neither
	//
	// In either order, in both modes, reported on stderr.

	const Result nv = Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet");
	const Result prime = Result::prime(WORK_LL, 31);
	const Result probable = Result::probable_prime(WORK_LL, 31);
	const Result composite = Result::composite_by_factor(WORK_TF, 31, "233");
	const Result by_residue = Result::composite_by_residue(WORK_LL, 31, 0x0123'4567'89AB'CDEF);

	struct Run {
		std::string label;
		std::vector<Result> results;
		std::vector<Result> printed;
	};
	const std::vector<Run> runs = {
		{"a prime, a composite",                {prime, composite},      {}},
		{"a composite, a prime",                {composite, prime},      {}},
		{"a probable prime, a composite",       {probable, by_residue},  {}},
		{"a composite, a probable prime",       {composite, probable},   {}},
		{"a no verdict, a prime, a composite",  {nv, prime, composite},  {nv}},
	};

	for (const bool json: {false, true})
		for (const Run& run: runs) {
			check::expect_equal(status_of(run.results), 30, run.label + ": status_of");
			const Printed printed = print_run(run.results, json);
			std::string expected;
			for (const Result& result: run.printed)
				expected += shown(result, json);
			check::expect_equal(printed.status, 30, run.label + ": the printer's status" + mode(json));
			check::expect_equal(printed.out, expected, run.label + ": stdout" + mode(json));
			check::expect(printed.err.find("Contradictory") != std::string::npos, run.label + ": reported on stderr" + mode(json));
		}
}


static void the_printers_errors_log_in_the_shared_style() {
	//
	// The printer's error and contradiction lines log in the style set in another unit
	//
	// With the detailed style, a host and a task id set here, each `critical` line carries
	// the stamp; in the default style each is today's line exactly.

	const Result prime = Result::prime(WORK_LL, 31);
	const Result composite = Result::composite_by_factor(WORK_TF, 31, "233");
	const std::string error_line = "critical [mps] M31 ll: error, a constructed failure" + tail() + "\n";
	const std::string contradiction_line = "critical [mps] Contradictory results, no verdict printed: M31 ll: prime" + tail() +
	                                       "; M31 tf: composite, factor 233" + tail() + "\n";

	log_peer::SavedLogger saved;
	logging::level = logging::INFO;
	logging::style = logging::DETAILED;
	logging::hostname = "check-host";
	logging::task_id = "abc";

	for (const bool json: {false, true}) {
		const Printed error = print_run({an_error()}, json);
		check::expect(log_peer::stamped(log_peer::plain(error.err), "check-host [abc] " + error_line),
		              "an error's line is stamped with the host and the task id" + mode(json));
		const Printed contradiction = print_run({prime, composite}, json);
		check::expect(log_peer::stamped(log_peer::plain(contradiction.err), "check-host [abc] " + contradiction_line),
		              "a contradiction's line is stamped with the host and the task id" + mode(json));
	}

	logging::style = logging::DEFAULT;
	for (const bool json: {false, true}) {
		check::expect_equal(log_peer::plain(print_run({an_error()}, json).err), error_line,
		                    "an error's line in the default style" + mode(json));
		check::expect_equal(log_peer::plain(print_run({prime, composite}, json).err), contradiction_line,
		                    "a contradiction's line in the default style" + mode(json));
	}
}


static void a_sound_run_prints_every_result_in_order() {
	//
	// Runs with no error and no contradiction print every result, in order
	//
	// The status is the first decisive result's, or 42 with none, and nothing goes to stderr.

	const Result nv = Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet");
	const Result prime = Result::prime(WORK_LL, 31);
	const Result probable = Result::probable_prime(WORK_LL, 31);
	const Result composite = Result::composite_by_factor(WORK_PTEST, 33, "2^3-1");

	struct Run {
		std::string label;
		std::vector<Result> results;
		int status;
	};
	const std::vector<Run> runs = {
		{"two no verdicts, a prime",   {nv, nv, prime},     0},
		{"a no verdict, a composite",  {nv, composite},     40},
		{"a no verdict, a probable prime", {nv, probable},  41},
		{"a composite, a no verdict",  {composite, nv},     40},
		{"three no verdicts",          {nv, nv, nv},        42},
		{"no result",                  {},                  42},
	};

	for (const bool json: {false, true})
		for (const Run& run: runs) {
			const Printed printed = print_run(run.results, json);
			std::string expected;
			for (const Result& result: run.results)
				expected += shown(result, json);
			check::expect_equal(printed.status, run.status, run.label + ": the status" + mode(json));
			check::expect_equal(printed.out, expected, run.label + ": stdout" + mode(json));
			check::expect_equal(printed.err, std::string(""), run.label + ": nothing on stderr" + mode(json));
		}
}


static void a_verdict_waits_for_the_run_to_end() {
	//
	// A no verdict prints as it arrives; a verdict waits for `report_run`

	const Result nv = Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet");
	const Result prime = Result::prime(WORK_LL, 31);

	for (const bool json: {false, true}) {
		std::ostringstream out;
		std::vector<Result> run;
		report_result(run, nv, json, out);
		check::expect_equal(out.str(), shown(nv, json), "a no verdict prints at once" + mode(json));
		report_result(run, prime, json, out);
		check::expect_equal(out.str(), shown(nv, json), "a prime is held" + mode(json));
		check::expect_equal(report_run(run, json, out), 0, "the held prime's status" + mode(json));
		check::expect_equal(out.str(), shown(nv, json) + shown(prime, json), "the held prime prints at the end" + mode(json));
	}
}


// The constructed tests' calls, in order, so a check sees which ran and when
static std::vector<std::string> ran;


static Result gives_no_verdict(const long unsigned p) {
	ran.push_back("no verdict");
	return Result::no_verdict(WORK_TF, p, "constructed");
}


static Result gives_prime(const long unsigned p) {
	ran.push_back("prime");
	return Result::prime(WORK_LL, p);
}


static Result gives_probable_prime(const long unsigned p) {
	ran.push_back("probable prime");
	return Result::probable_prime(WORK_LL, p);
}


static Result gives_composite(const long unsigned p) {
	ran.push_back("composite");
	return Result::composite_by_factor(WORK_P1, p, "3");
}


static Result gives_error(const long unsigned p) {
	ran.push_back("error");
	return Result::error(WORK_LL, p, "constructed");
}


static void run_in_order_stops_at_the_first_verdict() {
	//
	// Tests run in order, each result handed over before the next test starts
	//
	// The run stops after a prime, a probable prime or a composite, and goes on past a no
	// verdict or an error. The last result handed over is returned.

	struct Order {
		std::vector<TestFunction> tests;
		std::vector<std::string> handed;
	};
	const std::vector<Order> orders = {
		{{gives_no_verdict, gives_composite, gives_prime},        {"no verdict", "composite"}},
		{{gives_prime, gives_no_verdict, gives_no_verdict},       {"prime"}},
		{{gives_no_verdict, gives_probable_prime, gives_no_verdict}, {"no verdict", "probable prime"}},
		{{gives_no_verdict, gives_no_verdict, gives_prime},       {"no verdict", "no verdict", "prime"}},
		{{gives_no_verdict, gives_no_verdict, gives_no_verdict},  {"no verdict", "no verdict", "no verdict"}},
		{{gives_error, gives_no_verdict, gives_composite, gives_no_verdict}, {"error", "no verdict", "composite"}},
		{{gives_composite},                                       {"composite"}},
	};

	for (const Order& order: orders) {
		ran.clear();
		std::vector<std::string> handed;
		const Result last = run_in_order(7, order.tests, [&](const Result& result) {
			handed.push_back(ran.back());
			check::expect_equal(ran.size(), handed.size(), "a result is handed over before the next test runs");
			check::expect_equal(result.exponent(), 7ul, "the result is for the exponent run");
		});
		const std::string label = "the run starting with " + order.handed.front();
		check::expect(handed == order.handed, label + ": the results handed over");
		check::expect(ran == order.handed, label + ": no test runs after a verdict");
		check::expect_equal(verdict_line(last), verdict_line(order.tests[order.handed.size() - 1](7)), label + ": the last result returned");
	}
}


static void test_all_runs_the_three_tests_in_order() {
	//
	// Trial factoring, P-1 and Lucas-Lehmer, each handing over its no verdict
	//
	// None decides, so all three run, and each logs its start line first.

	std::vector<std::string> handed;
	std::ostringstream err;
	std::streambuf* saved = std::cerr.rdbuf(err.rdbuf());
	const Result last = test_all(31, [&](const Result& result) { handed.push_back(verdict_line(result)); });
	std::cerr.rdbuf(saved);

	const std::vector<std::string> expected = {
		"M31 tf: no verdict, trial factoring is not implemented yet" + tail(),
		"M31 p1: no verdict, P-1 is not implemented yet" + tail(),
		"M31 ll: no verdict, Lucas-Lehmer is not implemented yet" + tail(),
	};
	check::require(handed.size() == expected.size(), "test_all hands over three results");
	for (size_t i = 0; i < expected.size(); i++)
		check::expect_equal(handed[i], expected[i], "test_all's result " + std::to_string(i + 1));
	check::expect_equal(verdict_line(last), expected.back(), "test_all returns the last");

	const std::string log = err.str();
	const size_t tf = log.find("Running quick factor test");
	const size_t p1 = log.find("Running P-1 test");
	const size_t ll = log.find("Running Lucas-Lehmer test");
	check::expect(tf != std::string::npos and tf < p1, "trial factoring starts before P-1");
	check::expect(p1 != std::string::npos and p1 < ll, "P-1 starts before Lucas-Lehmer");
	check::expect(ll != std::string::npos, "Lucas-Lehmer starts");
}


static check::Suite
check_by_name {
	{"each outcome has its line",                    each_outcome_has_its_line},
	{"each outcome has its record",                  each_outcome_has_its_record},
	{"an error has a line and no record",            an_error_has_a_line_and_no_record},
	{"residues are sixteen uppercase digits",        residues_are_sixteen_uppercase_digits},
	{"every record is valid JSON",                   every_record_is_valid_json},
	{"the JSON reader rejects bad text",             the_json_reader_rejects_bad_text},
	{"a hostile reason stays one record",            a_hostile_reason_stays_one_record},
	{"a hostile reason stays one line",              a_hostile_reason_stays_one_line},
	{"a control-byte reason reads back",             a_control_byte_reason_reads_back_from_its_record},
	{"well-formed UTF-8 passes through",             well_formed_utf8_passes_through},
	{"malformed UTF-8 becomes U+FFFD",               a_malformed_utf8_reason_becomes_replacement_characters},
	{"only verdicts are decisive",                   only_verdicts_are_decisive},
	{"each outcome has its status",                  each_outcome_has_its_status},
	{"the status table keeps its values",            the_status_table_keeps_its_values},
	{"no status is reserved for burn-in",            no_status_is_reserved_for_burn_in},
	{"the printer shows each outcome",               the_printer_shows_each_outcome},
	{"an error prints no verdict",                   an_error_prints_no_verdict},
	{"a contradiction prints no verdict",            a_contradiction_prints_no_verdict},
	{"the printer's errors log in the shared style", the_printers_errors_log_in_the_shared_style},
	{"a sound run prints every result in order",     a_sound_run_prints_every_result_in_order},
	{"a verdict waits for the run to end",           a_verdict_waits_for_the_run_to_end},
	{"run_in_order stops at the first verdict",      run_in_order_stops_at_the_first_verdict},
	{"test_all runs the three tests in order",       test_all_runs_the_three_tests_in_order},
};
