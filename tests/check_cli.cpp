// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: command-line interface
//
// Only options that parse correctly today are pinned here; the known parse bugs are
// recorded in `issues/`.
//


// Standard libs
#include <string>

// Internal libs
#include "check.hpp"
#include "cli.hpp"


static void parse_args_accepts_test_and_pval() {
	//
	// `mps -t ll 31` selects Lucas-Lehmer on 31

	const char* argv[] = {"mps", "-t", "ll", "31"};
	cli::Interface opt = cli::parse_args(4, argv);

	check::expect_equal(opt.pval, std::string("31"), "PVAL is 31");
	check::expect(opt.test_case == cli::TEST_LUCAS_LEHMER, "-t ll selects Lucas-Lehmer");
	check::expect(opt.opt_test_case, "-t is recorded as given");
	check::expect(not opt.show_primes, "-p is not set");
}


static void parse_args_rejects_unknown_option() {
	//
	// An unknown option throws `cli::argument_error`

	for (const char* option: {"--no-such-option", "-x"}) {
		const char* argv[] = {"mps", option, "31"};
		bool rejected = false;
		try {
			cli::parse_args(3, argv);
		} catch (cli::argument_error const&) {
			rejected = true;
		}
		check::expect(rejected, std::string(option) + " throws cli::argument_error");
	}
}


static check::Suite
check_by_name {
	{"parse_args accepts -t ll 31",          parse_args_accepts_test_and_pval},
	{"parse_args rejects an unknown option", parse_args_rejects_unknown_option},
};
