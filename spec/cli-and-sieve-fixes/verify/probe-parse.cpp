// Gate probe of the parser: a cycle record, never part of mps.
//
// Linked against the tree's own modules (everything in src/ but main.cpp), it reports what
// cli::parse_args makes of its arguments, independently of the tree's checks:
//
//   probe-parse ARG...   one line per field of cli::Interface, or "info: <what>",
//                        "error: <what>" or "uncaught: <what>" when it throws


// Standard libs
#include <iostream>
#include <string>
#include <vector>

// Internal libs
#include "cli.hpp"


static const char* test_name(cli::TestCase t) {
	if (t == cli::TEST_ALL) return "all";
	if (t == cli::TEST_FACTOR) return "factor";
	if (t == cli::TEST_P_MINUS_ONE) return "p-minus-one";
	if (t == cli::TEST_LUCAS_LEHMER) return "lucas-lehmer";
	return "other";
}


int main(int argc, char** argv) {

	std::vector<const char*> args = {"mps"};
	for (int i = 1; i < argc; i++)
		args.push_back(argv[i]);
	try {
		cli::Interface o = cli::parse_args(static_cast<int>(args.size()), args.data());
		std::cout << "pval=" << o.pval << "\n"
		          << "pval_number=" << o.pval_number << "\n"
		          << "test_case=" << test_name(o.test_case) << "\n"
		          << "opt_test_case=" << o.opt_test_case << "\n"
		          << "file_path=" << o.file_path << "\n"
		          << "opt_file_path=" << o.opt_file_path << "\n"
		          << "show_primes=" << o.show_primes << "\n"
		          << "skip_ptest=" << o.skip_ptest << "\n"
		          << "debug_mode=" << o.debug_mode << "\n"
		          << "detailed_mode=" << o.detailed_mode << std::endl;
	} catch (cli::show_info const& e) {
		std::cout << "info: " << e.what() << std::endl;
	} catch (cli::argument_error const& e) {
		std::cout << "error: " << e.what() << std::endl;
	} catch (std::exception const& e) {
		std::cout << "uncaught: " << e.what() << std::endl;
	}
	return 0;
}
