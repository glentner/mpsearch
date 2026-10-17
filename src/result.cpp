// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Results
//
// Nothing here prints a verdict that the run has not earned: an error is never a verdict, a
// prime and a composite for the same exponent are a contradiction, and either one stops every
// verdict of the run from printing.
//


// Standard libs
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Internal libs
#include "main.hpp"
#include "log.hpp"
#include "mp.hpp"
#include "result.hpp"


Result::Result(Outcome outcome, Worktype worktype, long unsigned p)
	: outcome_(outcome), worktype_(worktype), exponent_(p) {}


Result Result::prime(Worktype worktype, long unsigned p) {
	return Result(OUTCOME_PRIME, worktype, p);
}


Result Result::probable_prime(Worktype worktype, long unsigned p) {
	return Result(OUTCOME_PROBABLE_PRIME, worktype, p);
}


Result Result::composite_by_factor(Worktype worktype, long unsigned p, const std::string& factor) {
	//
	// A composite, by a factor already re-checked exactly
	//
	// The factor is written exactly as a string, so one too large for a machine word still
	// fits.

	Result result(OUTCOME_COMPOSITE, worktype, p);
	result.factors_.push_back(factor);
	return result;
}


Result Result::composite_by_residue(Worktype worktype, long unsigned p, std::uint64_t res64) {
	//
	// A composite, by the nonzero final residue of a run whose every error check passed

	Result result(OUTCOME_COMPOSITE, worktype, p);
	result.res64_ = res64;
	return result;
}


Result Result::no_verdict(Worktype worktype, long unsigned p, const std::string& reason) {
	Result result(OUTCOME_NO_VERDICT, worktype, p);
	result.reason_ = reason;
	return result;
}


Result Result::error(Worktype worktype, long unsigned p, const std::string& reason) {
	Result result(OUTCOME_ERROR, worktype, p);
	result.reason_ = reason;
	return result;
}


// The outcome as the verdict line words it
static std::map<Outcome, std::string>
word_by_outcome {
	{OUTCOME_PRIME,           "prime"},
	{OUTCOME_PROBABLE_PRIME,  "probable prime"},
	{OUTCOME_COMPOSITE,       "composite"},
	{OUTCOME_NO_VERDICT,      "no verdict"},
	{OUTCOME_ERROR,           "error"},
};


// The outcome as the JSON record's status words it (an error has no record)
static std::map<Outcome, std::string>
label_by_outcome {
	{OUTCOME_PRIME,           "prime"},
	{OUTCOME_PROBABLE_PRIME,  "probable-prime"},
	{OUTCOME_COMPOSITE,       "composite"},
	{OUTCOME_NO_VERDICT,      "no-verdict"},
};


static std::string line_text(const std::string& text) {
	//
	// Text as a verdict line holds it
	//
	// Each control byte below 0x20, and DEL, is written `\x` with two lowercase hexadecimal
	// digits, so no text can end its line early, begin another line, or reach a terminal as a
	// control sequence. Every other byte passes through unchanged, so plain text and UTF-8
	// read as they were given.

	static const char DIGITS[] = "0123456789abcdef";
	std::string out;
	for (const unsigned char c: text) {
		if (c < 0x20 or c == 0x7f) {
			out += "\\x";
			out += DIGITS[c >> 4];
			out += DIGITS[c & 0xf];
		} else {
			out += char(c);
		}
	}
	return out;
}


static size_t utf8_length(const std::string& text, size_t at) {
	//
	// The length of the well-formed UTF-8 sequence that starts at `at`, or 0 if none does
	//
	// Follows the table of well-formed byte sequences in RFC 3629 (Yergeau, "UTF-8, a
	// transformation format of ISO 10646", 2003, section 4): the bounds on the second byte
	// exclude the overlong forms, the surrogates U+D800 to U+DFFF and everything beyond
	// U+10FFFF, and a sequence cut short by the end of the text is not well formed.

	const unsigned char lead = text[at];
	if (lead < 0x80)
		return 1;

	size_t length = 0;
	unsigned char low = 0x80;
	unsigned char high = 0xbf;
	if (lead >= 0xc2 and lead <= 0xdf) {
		length = 2;
	} else if (lead == 0xe0) {
		length = 3;
		low = 0xa0;
	} else if (lead >= 0xe1 and lead <= 0xef) {
		length = 3;
		if (lead == 0xed)
			high = 0x9f;
	} else if (lead == 0xf0) {
		length = 4;
		low = 0x90;
	} else if (lead >= 0xf1 and lead <= 0xf4) {
		length = 4;
		if (lead == 0xf4)
			high = 0x8f;
	} else {
		return 0;
	}

	if (length > text.size() - at)
		return 0;
	for (size_t i = 1; i < length; i++) {
		const unsigned char c = text[at + i];
		if (c < (i == 1 ? low : 0x80) or c > (i == 1 ? high : 0xbf))
			return 0;
	}
	return length;
}


static std::string json_text(const std::string& text) {
	//
	// A JSON string literal for text
	//
	// Escapes the quote, the backslash and each control character below 0x20 (RFC 8259, 2017,
	// section 7), so any text stays one string on one line. Well-formed UTF-8 passes through
	// unchanged, and each byte that neither begins nor continues a well-formed sequence
	// becomes U+FFFD, the replacement character, since JSON text is UTF-8 (RFC 8259, 2017,
	// section 8.1). Well-formed text encodes as it always has, and malformed text never gave a
	// valid record at all, so the record's format is unchanged and `RECORD_VERSION` with it.

	static const char DIGITS[] = "0123456789abcdef";
	static const std::string REPLACEMENT = "\xef\xbf\xbd";  // U+FFFD in UTF-8
	std::string out = "\"";
	size_t at = 0;
	while (at < text.size()) {
		const unsigned char c = text[at];
		const size_t length = utf8_length(text, at);
		if (length == 0) {
			out += REPLACEMENT;
			at++;
			continue;
		}
		if (c == '"' or c == '\\') {
			out += '\\';
			out += char(c);
		} else if (c < 0x20) {
			out += "\\u00";
			out += DIGITS[c >> 4];
			out += DIGITS[c & 0xf];
		} else {
			out += text.substr(at, length);
		}
		at += length;
	}
	return out + "\"";
}


static std::string joined(const std::vector<std::string>& parts, const std::string& between) {
	std::string text;
	for (size_t i = 0; i < parts.size(); i++)
		text += (i == 0 ? "" : between) + parts[i];
	return text;
}


bool is_decisive(const Result& result) {
	//
	// Whether a result decides its exponent
	//
	// Prime, probable prime and composite decide. A no verdict decides nothing, and an error is
	// never a verdict.

	switch (result.outcome()) {
		case OUTCOME_PRIME:
		case OUTCOME_PROBABLE_PRIME:
		case OUTCOME_COMPOSITE:
			return true;
		case OUTCOME_NO_VERDICT:
		case OUTCOME_ERROR:
			return false;
	}
	return false;
}


std::string verdict_line(const Result& result) {
	//
	// One line for a result, without its newline
	//
	// `M<p> <worktype>: <outcome>[, <detail>] (mps <version>)`, the exponent written from the
	// parsed number. The detail is `factor <f>` or `res64 <16 hex digits>` for a composite, and
	// the reason for a no verdict or an error, its control bytes escaped so that the line
	// stays one line. An error's line goes only to stderr.

	std::string line = "M" + std::to_string(result.exponent()) + " " + name_by_worktype[result.worktype()] + ": " +
	                   word_by_outcome[result.outcome()];

	if (result.outcome() == OUTCOME_COMPOSITE) {
		if (result.res64())
			line += ", res64 " + hex64(*result.res64());
		else
			line += ", factor " + joined(result.factors(), ", ");
	} else if (result.outcome() == OUTCOME_NO_VERDICT or result.outcome() == OUTCOME_ERROR) {
		line += ", " + line_text(result.reason());
	}

	return line + " (" + APP_NAME + " " + APP_VERSION + ")";
}


std::string json_record(const Result& result) {
	//
	// One JSON object for a result, on one line, without its newline
	//
	// The record's format: the keys `exponent` (a number), `worktype` (`tf`, `p1`, `ll`, or
	// `ptest` for the exponent check), `status` (`prime`, `probable-prime`, `composite` or
	// `no-verdict`), then `factors` (an array of exact strings) for a composite by factor,
	// `res64` (16 uppercase hexadecimal digits) for a composite by residue, or `reason` for a
	// no verdict, then `program` and `record_version`, in that order and spaced as Python's
	// `json.dumps` writes them. The exponent check's factor is written `2^a-1`, never in
	// decimal: a decimal form waits for the big integer, and would change the format.
	//
	// `record_version` is this format's own version, `RECORD_VERSION`; any change to the
	// record bumps it in the same commit. `program.version` is the program's, never the
	// format's. An error has no record: its text is empty, and the printer never asks for it.

	if (result.outcome() == OUTCOME_ERROR)
		return "";

	std::string record = "{\"exponent\": " + std::to_string(result.exponent()) +
	                     ", \"worktype\": " + json_text(name_by_worktype[result.worktype()]) +
	                     ", \"status\": " + json_text(label_by_outcome[result.outcome()]);

	if (result.outcome() == OUTCOME_COMPOSITE) {
		if (result.res64()) {
			record += ", \"res64\": " + json_text(hex64(*result.res64()));
		} else {
			std::vector<std::string> factors;
			for (const std::string& factor: result.factors())
				factors.push_back(json_text(factor));
			record += ", \"factors\": [" + joined(factors, ", ") + "]";
		}
	} else if (result.outcome() == OUTCOME_NO_VERDICT) {
		record += ", \"reason\": " + json_text(result.reason());
	}

	return record + ", \"program\": {\"name\": " + json_text(APP_NAME) + ", \"version\": " + json_text(APP_VERSION) +
	       "}, \"record_version\": " + std::to_string(RECORD_VERSION) + "}";
}


int status_of(const std::vector<Result>& results) {
	//
	// The exit status of a run
	//
	// A prime or a probable prime together with a composite is a contradiction, an internal
	// failure; otherwise any error makes the run a runtime error. Neither is a verdict. Else
	// the run takes the status of its first decisive result, and with none it is no verdict.
	// Each status comes from a switch, never a table, whose missing key would read as 0, prime.

	bool primal = false;
	bool composite = false;
	bool failed = false;
	for (const Result& result: results) {
		switch (result.outcome()) {
			case OUTCOME_PRIME:
			case OUTCOME_PROBABLE_PRIME:
				primal = true;
				break;
			case OUTCOME_COMPOSITE:
				composite = true;
				break;
			case OUTCOME_ERROR:
				failed = true;
				break;
			case OUTCOME_NO_VERDICT:
				break;
		}
	}

	if (primal and composite)
		return ERR_UNKNOWN;
	if (failed)
		return ERR_RUN;

	for (const Result& result: results) {
		switch (result.outcome()) {
			case OUTCOME_PRIME:
				return OK;
			case OUTCOME_PROBABLE_PRIME:
				return PROBABLE_PRIME;
			case OUTCOME_COMPOSITE:
				return COMPOSITE;
			case OUTCOME_NO_VERDICT:
			case OUTCOME_ERROR:
				break;
		}
	}
	return NO_VERDICT;
}


void report_result(std::vector<Result>& run, const Result& result, bool json, std::ostream& out) {
	//
	// Take one result as it arrives
	//
	// The result joins the run. An error is reported on stderr and never printed on `out`. A
	// no verdict prints at once, so its line appears as its test completes, unless a verdict
	// is already held, whose line must come first. A verdict prints nothing yet: only
	// `report_run` knows whether the run holds no error and no contradiction.

	bool held = false;
	for (const Result& earlier: run)
		if (is_decisive(earlier))
			held = true;

	run.push_back(result);

	if (result.outcome() == OUTCOME_ERROR) {
		logging::critical(verdict_line(result));
		return;
	}

	if (result.outcome() == OUTCOME_NO_VERDICT and not held)
		out << (json ? json_record(result) : verdict_line(result)) << std::endl;
}


int report_run(const std::vector<Result>& run, bool json, std::ostream& out) {
	//
	// Finish a run, returning its exit status
	//
	// On an error or a contradiction no verdict prints, and a contradiction is reported on
	// stderr (an error already was, as it arrived). Otherwise every result from the first
	// decisive one on prints, in the order the tests ran.

	const int status = status_of(run);

	if (status == ERR_UNKNOWN) {
		std::vector<std::string> lines;
		for (const Result& result: run)
			if (is_decisive(result))
				lines.push_back(verdict_line(result));
		logging::critical("Contradictory results, no verdict printed: ", joined(lines, "; "));
		return status;
	}

	if (status == ERR_RUN)
		return status;

	bool decided = false;
	for (const Result& result: run) {
		decided = decided or is_decisive(result);
		if (decided)
			out << (json ? json_record(result) : verdict_line(result)) << std::endl;
	}
	return status;
}
