// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Remediation probe (review cycle 1, F1): hostile reasons through the tree's own verdict_line and
// json_record. For each reason it prints four lines of hex: the reason, the no-verdict line, the
// no-verdict record and the error line. remediation/hostile.py judges them.
//


// Standard libs
#include <cstdio>
#include <string>
#include <vector>

// Internal libs
#include "result.hpp"


static void hex(const std::string& text) {
	for (const unsigned char c: text)
		std::printf("%02x", c);
	std::printf("\n");
}


int main() {
	const std::vector<std::string> reasons {
		"plain reason, as a test would give it",
		"caf\xc3\xa9, \xe2\x82\xac and \xf0\x9d\x84\x9e",
		"line one\nM31 ll: prime (mps 0.1.0)",
		"carriage\rreturn",
		"tab\there",
		"escape \x1b[31mred\x1b[0m",
		std::string("nul\0inside", 10),
		"delete\x7f",
		"quote \" and backslash \\ inside",
		"\xff",
		"\x80 lone continuation",
		"truncated \xc3",
		"bad continuation \xc3(",
		"overlong \xc0\xaf",
		"surrogate \xed\xa0\x80",
		"beyond \xf4\x90\x80\x80",
	};
	for (const std::string& reason: reasons) {
		hex(reason);
		hex(verdict_line(Result::no_verdict(WORK_TF, 31, reason)));
		hex(json_record(Result::no_verdict(WORK_TF, 31, reason)));
		hex(verdict_line(Result::error(WORK_LL, 31, reason)));
	}
	return 0;
}
