#!/bin/sh
# P1 gate: the Makefile's settings record, AGENTS.md and .gitignore (R1-R9).
# The edited files as PLAN § 2 pins them, nothing else changed; with GNU make 3.81, clang++ and
# g++-16: the CI probe and a fresh clone's make and make check as at b3a6c5a; no-op runs that call
# no compiler and touch no file; touches that rebuild what b3a6c5a rebuilds; every switch of
# compiler (by argument or environment) or of CXXFLAGS, CPPFLAGS, IEEE or BOUNDS, and every return,
# byte for byte what a fresh make -B with those settings makes; git sees only lib/ and bin/; make
# clean leaves no file; R4's time; the generators' imports leave git clean (PLAN § 6).
set -u
PHASE=1
. "$(dirname "$0")/common.sh"

gate_all
gate_end "P1 gate"
