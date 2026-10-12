// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception


// Standard libs
#include <iostream>
#include <string>
#include <exception>
#include <optional>
#include <vector>

// Internal libs
#include "main.hpp"
#include "ansi.hpp"
#include "log.hpp"
#include "cli.hpp"
#include "math.hpp"
#include "mp.hpp"
#include "result.hpp"


int main(const int argc, const char **argv) {

	logging::init();

	try {

		auto opt = cli::parse_args(argc, argv);

		if (opt.detailed_mode)
			logging::set_style(logging::DETAILED);

		if (opt.debug_mode)
			logging::set_level(logging::DEBUG);

		if (opt.show_primes) {
			display_primes(opt.pval_number);
			return OK;
		}

		const long unsigned pval = opt.pval_number;

		// Every result goes to the printer as its test completes; the printer decides the
		// status and prints a verdict only once the run is known to be sound
		std::vector<Result> run;
		const ResultSink completed = [&](const Result& result) {
			report_result(run, result, opt.json_mode, std::cout);
		};

		// Unless -s says otherwise, the exponent check comes first: a composite exponent is
		// answered before any test runs, and that answer is the run's only result
		std::optional<Result> answer;
		if (not opt.skip_ptest)
			answer = check_exponent(pval);

		if (answer) {
			completed(*answer);
		} else {
			switch (opt.test_case) {

				case cli::TEST_FACTOR:
					completed(test_quick_factor(pval));
					break;

				case cli::TEST_P_MINUS_ONE:
					completed(test_p_minus_one(pval));
					break;

				case cli::TEST_LUCAS_LEHMER:
					completed(test_lucas_lehmer(pval));
					break;

				case cli::TEST_ALL:
					test_all(pval, completed);
					break;
			}
		}

		return report_run(run, opt.json_mode, std::cout);

	} catch (cli::show_info const& error) {
		std::cout << colorize_usage(error.what()) << std::endl;
		return OK;

	} catch (cli::argument_error const& error) {
		logging::critical(error.what());
		return ERR_ARG;

	} catch (std::runtime_error const& error) {
		logging::critical(error.what());
		return ERR_RUN;

	} catch (std::exception const& error) {
		logging::critical("Uncaught exception: ", error.what());
		return ERR_UNKNOWN;
	}

    return OK;
}


