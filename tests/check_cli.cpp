// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: command-line interface
//
// Every option `--help` lists, in every spelling it states, parses to the same fields; the
// values it does not list are argument errors naming them, and so is a bad PVAL. Parsing
// is checked through `cli::parse_args` alone, which touches no file.
//


// Standard libs
#include <exception>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "cli.hpp"
#include "main.hpp"


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


static std::string parse_error(std::vector<const char*> argv) {
	//
	// The message `cli::parse_args` throws for these arguments
	//
	// Empty when they parse, so a missing error fails any check on its text. Anything thrown
	// but a `cli::argument_error` comes back marked as such, failing those checks too.

	try {
		cli::parse_args(static_cast<int>(argv.size()), argv.data());

	} catch (cli::argument_error const& error) {
		return error.what();

	} catch (std::exception const& error) {
		return std::string("not an argument error: ") + error.what();
	}
	return "";
}


static std::string words(const std::vector<const char*>& argv) {
	//
	// The arguments as one line, for labels

	std::string line;
	for (const char* word: argv)
		line += std::string(line.empty() ? "" : " ") + "'" + word + "'";
	return line;
}


static void parse_args_rejects_bad_pvals() {
	//
	// A bad PVAL is an argument error naming PVAL and quoting the value
	//
	// PVAL is the last word of each. Without -p it must be ASCII digits whose value is from 2
	// to 999999999; with -p, from 0. A value past 2^64 must not wrap into range.

	static const std::vector<std::vector<const char*>> rejected = {
		{"mps", "abc"},
		{"mps", "31abc"},
		{"mps", "3.5"},
		{"mps", " 31"},
		{"mps", "31 "},
		{"mps", "+31"},
		{"mps", ""},
		{"mps", "\u0663"},  // ARABIC-INDIC DIGIT THREE
		{"mps", "0"},
		{"mps", "1"},
		{"mps", "00"},
		{"mps", "1000000000"},
		{"mps", "18446744073709551647"},
		{"mps", "--", "-1"},
		{"mps", "-p", "abc"},
		{"mps", "-p", "1000000000"},
		{"mps", "-p", ""},
		{"mps", "-p", "18446744073709551616"},
		{"mps", "-p", "--", "-1"},
	};

	for (const std::vector<const char*>& argv: rejected) {
		const std::string quoted = std::string("PVAL '") + argv.back() + "'";
		check::expect(parse_error(argv).find(quoted) != std::string::npos,
		              words(argv) + " is an argument error naming " + quoted);
	}
}


static void parse_args_states_the_pval_bounds() {
	//
	// The PVAL error states the range, which -p widens

	check::expect_equal(parse_error({"mps", "abc"}),
	                    std::string("Invalid PVAL 'abc': expected a decimal integer from 2 to 999999999"),
	                    "the PVAL error for 'abc'");
	check::expect_equal(parse_error({"mps", "-p", "abc"}),
	                    std::string("Invalid PVAL 'abc': expected a decimal integer from 0 to 999999999 with -p/--primes"),
	                    "the PVAL error for '-p' 'abc'");
}


static void parse_args_accepts_good_pvals() {
	//
	// A good PVAL parses to its value
	//
	// Leading zeros are allowed, the bound is inclusive, and -p counts wherever it is given.
	// The string is kept as typed.

	struct Good {
		std::vector<const char*> argv;
		std::string pval;
		long unsigned number;
	};

	static const std::vector<Good> accepted = {
		{{"mps", "2"},                 "2",           2},
		{{"mps", "999999999"},         "999999999",   999'999'999},
		{{"mps", "0000000031"},        "0000000031",  31},
		{{"mps", "--", "31"},          "31",          31},
		{{"mps", "-p", "0"},           "0",           0},
		{{"mps", "0", "-p"},           "0",           0},
		{{"mps", "--primes", "000"},   "000",         0},
		{{"mps", "-p", "999999999"},   "999999999",   999'999'999},
	};

	for (Good good: accepted) {
		const std::string label = words(good.argv);
		try {
			const cli::Interface opt = cli::parse_args(static_cast<int>(good.argv.size()), good.argv.data());
			check::expect_equal(opt.pval_number, good.number, label + " gives PVAL " + std::to_string(good.number));
			check::expect_equal(opt.pval, good.pval, label + " keeps PVAL as typed");

		} catch (std::exception const& error) {
			check::expect(false, label + " parses (" + error.what() + ")");
		}
	}
}


static void parse_args_names_the_default_file_from_pval() {
	//
	// The default checkpoint path is PVAL as typed, with `.mps`

	const char* argv[] = {"mps", "0000000031"};
	try {
		const cli::Interface opt = cli::parse_args(2, argv);
		check::expect_equal(opt.file_path, std::string("0000000031.mps"), "'0000000031' gives the path 0000000031.mps");

	} catch (std::exception const& error) {
		check::expect(false, std::string("'mps' '0000000031' parses (") + error.what() + ")");
	}
}


static std::string describe(const cli::Interface& opt) {
	//
	// Every field the options set, one per line
	//
	// Two parses that agree here agree on everything `main` reads, and a failed comparison
	// prints both.

	return "pval=" + opt.pval + "\n"
	       + "pval_number=" + std::to_string(opt.pval_number) + "\n"
	       + "test_case=" + std::to_string(opt.test_case) + "\n"
	       + "opt_test_case=" + std::to_string(opt.opt_test_case) + "\n"
	       + "file_path=" + opt.file_path + "\n"
	       + "opt_file_path=" + std::to_string(opt.opt_file_path) + "\n"
	       + "show_primes=" + std::to_string(opt.show_primes) + "\n"
	       + "skip_ptest=" + std::to_string(opt.skip_ptest) + "\n"
	       + "debug_mode=" + std::to_string(opt.debug_mode) + "\n"
	       + "detailed_mode=" + std::to_string(opt.detailed_mode) + "\n";
}


static std::string parsed(std::vector<const char*> argv) {
	//
	// What `cli::parse_args` makes of these arguments
	//
	// The fields as `describe` gives them, or what was thrown, marked by its kind, so that
	// a help, an error and a parse never compare equal.

	try {
		return describe(cli::parse_args(static_cast<int>(argv.size()), argv.data()));

	} catch (cli::show_info const& info) {
		return std::string("info: ") + info.what();

	} catch (cli::argument_error const& error) {
		return std::string("argument error: ") + error.what();

	} catch (std::exception const& error) {
		return std::string("exception: ") + error.what();
	}
}


// The five spellings of a valued option, as `--help` states them
static const std::vector<std::string> SPELLINGS = {"-x VALUE", "-xVALUE", "-x=VALUE", "--long VALUE", "--long=VALUE"};


static void spell(std::vector<std::string>& words, const std::string& spelling, char flag,
                  const std::string& name, const std::string& value) {
	//
	// Append a valued option to `words`, spelled as one of `SPELLINGS`

	const std::string short_option = std::string("-") + flag;
	if (spelling == "-x VALUE") {
		words.push_back(short_option);
		words.push_back(value);
	} else if (spelling == "-xVALUE") {
		words.push_back(short_option + value);
	} else if (spelling == "-x=VALUE") {
		words.push_back(short_option + "=" + value);
	} else if (spelling == "--long VALUE") {
		words.push_back("--" + name);
		words.push_back(value);
	} else {
		words.push_back("--" + name + "=" + value);
	}
}


static std::vector<const char*> pointers(const std::vector<std::string>& words) {
	//
	// The words as `argv`, valid while `words` lives

	std::vector<const char*> argv;
	for (const std::string& word: words)
		argv.push_back(word.c_str());
	return argv;
}


// The path the spelling checks give -f: its directory does not exist, and it holds an '='
static const std::string PATH = "no-such-dir/x=y.mps";


static void parse_args_spells_each_option_alike() {
	//
	// Each spelling of -t and -f sets exactly its value
	//
	// Every name, in each of the five spellings of -t, beside each of the five of -f, parses
	// as --test NAME --file PATH does. That baseline is checked on its own first, so the
	// comparisons cannot agree on nothing.

	struct Name {
		std::string name;
		cli::TestCase test_case;
	};

	static const std::vector<Name> names = {
		{"all",          cli::TEST_ALL},
		{"tf",           cli::TEST_FACTOR},
		{"trial-factor", cli::TEST_FACTOR},
		{"p1",           cli::TEST_P_MINUS_ONE},
		{"p-minus-1",    cli::TEST_P_MINUS_ONE},
		{"ll",           cli::TEST_LUCAS_LEHMER},
		{"lucas-lehmer", cli::TEST_LUCAS_LEHMER},
	};

	for (const Name& name: names) {
		const std::vector<std::string> base = {"mps", "--test", name.name, "--file", PATH, "31"};
		const std::string label = words(pointers(base));
		try {
			const cli::Interface opt = cli::parse_args(static_cast<int>(base.size()), pointers(base).data());
			check::expect_equal(opt.test_case, name.test_case, label + " selects its test");
			check::expect_equal(opt.file_path, PATH, label + " sets the path");
			check::expect(opt.opt_test_case and opt.opt_file_path, label + " records both options as given");

		} catch (std::exception const& error) {
			check::expect(false, label + " parses (" + error.what() + ")");
		}

		const std::string want = parsed(pointers(base));
		for (const std::string& test_spelling: SPELLINGS) {
			for (const std::string& file_spelling: SPELLINGS) {
				std::vector<std::string> argv = {"mps"};
				spell(argv, file_spelling, 'f', "file", PATH);
				spell(argv, test_spelling, 't', "test", name.name);
				argv.push_back("31");
				check::expect_equal(parsed(pointers(argv)), want, words(pointers(argv)) + " parses as " + label);
			}
		}
	}
}


static void parse_args_selects_all_tests_by_default() {
	//
	// Without -t, all three tests run

	const char* argv[] = {"mps", "31"};
	try {
		const cli::Interface opt = cli::parse_args(2, argv);
		check::expect_equal(opt.test_case, cli::TEST_ALL, "'mps' '31' selects all three tests");
		check::expect(not opt.opt_test_case, "'mps' '31' records no -t");

	} catch (std::exception const& error) {
		check::expect(false, std::string("'mps' '31' parses (") + error.what() + ")");
	}
}


static void parse_args_reads_file_before_test() {
	//
	// --file takes the next word as its path, so -t after it still selects the test

	const char* argv[] = {"mps", "--file", "out.mps", "-t", "ll", "31"};
	try {
		const cli::Interface opt = cli::parse_args(6, argv);
		check::expect_equal(opt.file_path, std::string("out.mps"), "--file out.mps sets the path");
		check::expect_equal(opt.test_case, cli::TEST_LUCAS_LEHMER, "--file out.mps -t ll selects Lucas-Lehmer");

	} catch (std::exception const& error) {
		check::expect(false, std::string("--file out.mps -t ll 31 parses (") + error.what() + ")");
	}
}


static void parse_args_takes_values_whole() {
	//
	// A value is the text after the option, less one '=' that joins them

	struct Value {
		std::vector<const char*> argv;
		std::string file_path;
	};

	static const std::vector<Value> values = {
		{{"mps", "-f=a=b", "31"},       "a=b"},
		{{"mps", "--file=a=b", "31"},   "a=b"},
		{{"mps", "--file", "a=b", "31"}, "a=b"},
		{{"mps", "-f", "=x", "31"},     "=x"},
		{{"mps", "-f==x", "31"},        "=x"},
		{{"mps", "--file==x", "31"},    "=x"},
		{{"mps", "-f==", "31"},         "="},
		{{"mps", "-fdD", "31"},         "dD"},
	};

	for (Value value: values) {
		const std::string label = words(value.argv);
		try {
			const cli::Interface opt = cli::parse_args(static_cast<int>(value.argv.size()), value.argv.data());
			check::expect_equal(opt.file_path, value.file_path, label + " sets the path " + value.file_path);

		} catch (std::exception const& error) {
			check::expect(false, label + " parses (" + error.what() + ")");
		}
	}
}


static void parse_args_rejects_unlisted_test_names() {
	//
	// A -t value `--help` does not list is an argument error quoting it, in every spelling
	//
	// Joined to -t, an empty value is -t alone and is left out.

	static const std::vector<std::string> rejected = {"qf", "quick", "factor", "pm", "p-min", "none", "",
	                                                  "no-such-test", "LL", "=ll"};

	for (const std::string& value: rejected) {
		for (const std::string& spelling: SPELLINGS) {
			if (spelling == "-xVALUE" and (value.empty() or value[0] == '='))
				continue;
			std::vector<std::string> argv = {"mps"};
			spell(argv, spelling, 't', "test", value);
			argv.push_back("31");
			check::expect_equal(parse_error(pointers(argv)),
			                    "Unrecognized option value '" + value + "' for -t/--test",
			                    words(pointers(argv)) + " is an argument error quoting '" + value + "'");
		}
	}
}


static void parse_args_rejects_missing_values() {
	//
	// A valued option with no value, or -f with an empty one, is an argument error

	struct Missing {
		std::vector<const char*> argv;
		std::string message;
	};

	static const std::vector<Missing> missing = {
		{{"mps", "31", "-t"},          "Missing value for -t/--test"},
		{{"mps", "31", "--test"},      "Missing value for -t/--test"},
		{{"mps", "31", "-dt"},         "Missing value for -t/--test"},
		{{"mps", "31", "-f"},          "Missing value for -f/--file"},
		{{"mps", "31", "--file"},      "Missing value for -f/--file"},
		{{"mps", "-f=", "31"},         "Empty value for -f/--file"},
		{{"mps", "--file=", "31"},     "Empty value for -f/--file"},
		{{"mps", "-f", "", "31"},      "Empty value for -f/--file"},
		{{"mps", "--debug=yes", "31"}, "Option --debug takes no value"},
	};

	for (const Missing& m: missing)
		check::expect_equal(parse_error(m.argv), m.message, words(m.argv) + " is an argument error");
}


static void parse_args_reads_groups_as_separate_flags() {
	//
	// Grouped flags parse as the same flags given separately, in order

	struct Group {
		std::vector<const char*> grouped;
		std::vector<const char*> separate;
	};

	static const std::vector<Group> groups = {
		{{"mps", "-dD", "31"},  {"mps", "-d", "-D", "31"}},
		{{"mps", "-Dd", "31"},  {"mps", "-D", "-d", "31"}},
		{{"mps", "-pd", "30"},  {"mps", "-p", "-d", "30"}},
		{{"mps", "-ps", "30"},  {"mps", "-p", "-s", "30"}},
		{{"mps", "-sdD", "31"}, {"mps", "-s", "-d", "-D", "31"}},
		{{"mps", "-hv"},        {"mps", "-h"}},
		{{"mps", "-vh"},        {"mps", "-v"}},
		{{"mps", "-hx"},        {"mps", "-h"}},
		{{"mps", "-dh", "31"},  {"mps", "-h"}},
	};

	for (const Group& group: groups)
		check::expect_equal(parsed(group.grouped), parsed(group.separate),
		                    words(group.grouped) + " parses as " + words(group.separate));

	const char* argv[] = {"mps", "-sdD", "31"};
	try {
		const cli::Interface opt = cli::parse_args(3, argv);
		check::expect(opt.skip_ptest, "-sdD sets -s");
		check::expect(opt.debug_mode, "-sdD sets -d");
		check::expect(opt.detailed_mode, "-sdD sets -D");

	} catch (std::exception const& error) {
		check::expect(false, std::string("-sdD 31 parses (") + error.what() + ")");
	}

	check::expect_equal(parsed({"mps", "-hv"}), "info: " + APP_HELP, "-hv is the help");
	check::expect_equal(parsed({"mps", "-vh"}), "info: " + APP_NAME + " v" + APP_VERSION, "-vh is the version");
}


static void parse_args_rejects_stray_flags_in_groups() {
	//
	// A character in a group that is not a flag is an argument error naming it

	check::expect_equal(parse_error({"mps", "-dx", "31"}), std::string("Unrecognized option: -x"), "-dx names -x");
	check::expect_equal(parse_error({"mps", "-xh"}), std::string("Unrecognized option: -x"), "-xh names -x");
	check::expect_equal(parse_error({"mps", "-pdZ", "30"}), std::string("Unrecognized option: -Z"), "-pdZ names -Z");
}


static check::Suite
check_by_name {
	{"parse_args accepts -t ll 31",                   parse_args_accepts_test_and_pval},
	{"parse_args rejects an unknown option",          parse_args_rejects_unknown_option},
	{"parse_args rejects bad PVALs",                  parse_args_rejects_bad_pvals},
	{"parse_args states the PVAL bounds",             parse_args_states_the_pval_bounds},
	{"parse_args accepts good PVALs",                 parse_args_accepts_good_pvals},
	{"parse_args names the default file from PVAL",   parse_args_names_the_default_file_from_pval},
	{"parse_args spells each option alike",           parse_args_spells_each_option_alike},
	{"parse_args selects all tests by default",       parse_args_selects_all_tests_by_default},
	{"parse_args reads --file before -t",             parse_args_reads_file_before_test},
	{"parse_args takes values whole",                 parse_args_takes_values_whole},
	{"parse_args rejects unlisted test names",        parse_args_rejects_unlisted_test_names},
	{"parse_args rejects missing values",             parse_args_rejects_missing_values},
	{"parse_args reads groups as separate flags",     parse_args_reads_groups_as_separate_flags},
	{"parse_args rejects stray flags in groups",      parse_args_rejects_stray_flags_in_groups},
};
