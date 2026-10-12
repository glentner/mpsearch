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
	COMPOSITE     = 40,  // Composite
	PROBABLE_PRIME = 41, // Probable prime
	NO_VERDICT    = 42,  // No verdict
};


static const std::string APP_USAGE = R"USAGE(Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]
  Check if number is Mersenne prime.)USAGE";


static const std::string APP_HELP = R"HELP(Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print the primes below PVAL.
  -s, --skip-ptest        Skip the exponent check (that PVAL is prime).
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -j, --json              Print each result as one JSON object per line.
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD).

Exit status:
  0       Prime, or no verdict by design (--help, --version, -p).
  1       Wrong command-line invocation.
  2       Configuration error.
  10      File I/O or formatting error.
  20      Runtime error.
  30      Unexpected internal failure.
  40      Composite.
  41      Probable prime.
  42      No verdict.
  50-59   Reserved for burn-in.)HELP";

