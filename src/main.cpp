// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPLv3


// Standard libs
#include <iostream>
#include <string>
#include <exception>

// Internal libs
#include "main.hpp"
#include "ansi.hpp"
#include "log.hpp"
#include "cli.hpp"
#include "math.hpp"
#include "mp.hpp"


int main(const int argc, const char **argv) {

	log::init();

	try {

		auto opt = cli::parse_args(argc, argv);

		if (opt.detailed_mode)
			log::set_style(log::DETAILED);

		if (opt.debug_mode)
			log::set_level(log::DEBUG);

		if (opt.show_primes) {
			display_primes(std::stoi(opt.pval));
			return OK;
		}

		const long unsigned pval = std::stoi(opt.pval);

		switch (opt.test_case) {

			case cli::TEST_FACTOR:
				test_quick_factor(pval);
				break;

			case cli::TEST_P_MINUS_ONE:
				test_p_minus_one(pval);
				break;

			case cli::TEST_LUCAS_LEHMER:
				test_lucas_lehmer(pval);
				break;

			default:
				test_all(pval);
		}

	} catch (cli::show_info const& error) {
		std::cout << colorize_usage(error.what()) << std::endl;
		return OK;

	} catch (cli::argument_error const& error) {
		log::critical(error.what());
		return ERR_ARG;

	} catch (std::runtime_error const& error) {
		log::critical(error.what());
		return ERR_RUN;

	} catch (std::exception const& error) {
		log::critical("Uncaught exception: ", error.what());
		return ERR_UNKNOWN;
	}

    return OK;
}


