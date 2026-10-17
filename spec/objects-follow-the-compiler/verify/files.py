# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# The files this cycle edits, held to PLAN § 2 (a cycle record, not a standing gate)
#
#   python3 -I -B files.py agents BASE_FILE TREE_FILE      AGENTS.md is b3a6c5a's with exactly the
#                                                          three edits PLAN § 2 pins (R8)
#   python3 -I -B files.py gitignore BASE_FILE TREE_FILE   .gitignore is b3a6c5a's with one line,
#                                                          __pycache__/, at its end (R9)
#   python3 -I -B files.py makefile TREE_FILE              the Makefile uses nothing newer than GNU
#                                                          make 3.81, which would expand to nothing
#                                                          there without a word (R6)

import re, sys

ROW_OLD = "| verify | `make check`: the checks and the smoke test, both always run (`make -B check` after changing `CXX`) |\n"
ROW_NEW = "| verify | `make check`: the checks and the smoke test, both always run |\n"

CLEAN_OLD = "make clean                # remove the objects in lib/ and the binaries in bin/\n"
CLEAN_NEW = ("make clean                # remove what the build wrote: the objects in lib/ and the record of\n"
             "                          # the settings that built them (lib/settings), the binaries in bin/\n")

PARA_OLD = ("The build needs only a C++20 compiler and GNU make, and `make check` adds only a POSIX shell. Apple\n"
            "clang and GCC must both build it: macOS ships GNU make 3.81, so keep the Makefile within it.\n")
PARA_NEW = PARA_OLD + (
    "A build whose compiler or compile flags (`CXX`, `CXXFLAGS`, `CPPFLAGS`, `IEEE`, `BOUNDS`) differ\n"
    "from those recorded in `lib/settings` first removes everything the last build made, so a switch\n"
    "needs neither `make -B` nor `make clean`. A compiler upgraded in place under the same name still\n"
    "needs `make -B`.\n")


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def read(p):
    with open(p, encoding="utf-8") as f:
        return f.read()


def agents(base_file, tree_file):
    base, tree = read(base_file), read(tree_file)
    for old in (ROW_OLD, CLEAN_OLD, PARA_OLD):
        if base.count(old) != 1:
            die("b3a6c5a's AGENTS.md does not hold the pinned line once: %r" % old[:60])
    if "make -B check" in tree:
        die("AGENTS.md still says `make -B check` (R8)")
    if tree.count(ROW_NEW) != 1:
        die("AGENTS.md's profile has no verify row reading exactly: %s (R8)" % ROW_NEW.strip())
    want = base.replace(ROW_OLD, ROW_NEW).replace(CLEAN_OLD, CLEAN_NEW).replace(PARA_OLD, PARA_NEW)
    if tree != want:
        w, t = want.splitlines(), tree.splitlines()
        for i, (a, b) in enumerate(zip(w, t)):
            if a != b:
                die("AGENTS.md line %d is not PLAN § 2's: want %r, got %r (R8)" % (i + 1, a[:80], b[:80]))
        die("AGENTS.md has %d lines, PLAN § 2 gives %d (R8)" % (len(t), len(w)))


def gitignore(base_file, tree_file):
    if read(tree_file) != read(base_file) + "__pycache__/\n":
        die(".gitignore is not b3a6c5a's with the one line __pycache__/ at its end (R9)")


def makefile(tree_file):
    text = read(tree_file)
    later = [(r"\$\(file\s", "$(file ...) (make 4.0)"), (r"^\s*[A-Za-z_][A-Za-z0-9_]*\s*!=", "!= (make 4.0)"),
             (r"::=", "::= (make 3.82)"), (r"^\s*undefine\b", "undefine (make 3.82)"),
             (r"^\.ONESHELL\b", ".ONESHELL (make 3.82)"), (r"\.RECIPEPREFIX", ".RECIPEPREFIX (make 3.82)"),
             (r"\$\(guile\s", "$(guile ...) (make 4.0)"), (r"^\s*private\s", "private (make 3.82)"),
             (r"\$\(let\s", "$(let ...) (make 4.4)"), (r"\$\(intcmp\s", "$(intcmp ...) (make 4.4)")]
    for pat, what in later:
        if re.search(pat, text, re.M):
            die("the Makefile uses %s, which GNU make 3.81 does not have (R6)" % what)


cmd, args = sys.argv[1], sys.argv[2:]
{"agents": agents, "gitignore": gitignore, "makefile": makefile}[cmd](*args)
