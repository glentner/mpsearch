// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Metadata and usage text
//

#pragma once


// Standard libs
#include <string>


static const std::string APP_NAME = "mps";
static const std::string APP_VERSION = "0.1.0";


enum Status {
	OK            = 0,   // Everything ran as expected
	ERR_ARG       = 1,   // Wrong command-line invocation
	ERR_CFG       = 2,   // Configuration error
	ERR_FILE      = 10,  // File I/O or formatting error
	ERR_RUN       = 20,  // Generic runtime error
	ERR_UNKNOWN   = 30,  // Unexpected internal failure
};


static const std::string APP_USAGE = R"USAGE(Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-f PATH] [-d] [-D]
  Check if number is Mersenne prime.)USAGE";


static const std::string APP_HELP = R"HELP(Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print primes up to PVAL.
  -s, --skip-ptest        Suppress initial prime test on PVAL.
  -t, --test        NAME  One of 'tf', 'p1', 'll' (default: 'all').
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.)HELP";

