# What a phase may change and what PLAN § 2 pins (a cycle record, not a standing gate). Run from the
# repository root with python3 -I.
#
#   ll_files.py changes PHASE < CHANGED     every changed path (spec/ aside) is one the phase may change
#   ll_files.py interface PHASE             the pinned declarations and definitions are present
#   ll_files.py labels PHASE                the pinned check labels, each once, in their file's table
#   ll_files.py agents PHASE BASE_AGENTS    AGENTS.md: the list only widens, the lines tests/cli.sh
#                                           reads and the verify row as at bcb6fdf, and the new names
#   ll_files.py makefile PHASE BASE_MAKE    the Makefile: flags as at bcb6fdf, the recipes PLAN § 2.6 names
#   ll_files.py source PHASE                the new sources' headers, no spec ids, the injections and the
#                                           tables out of bin/mps's reach (R16, § Invariants 3)
#   ll_files.py added COMMIT PATH...        COMMIT's AGENTS.md adds, against its parent's, a line naming
#                                           each PATH to the high-blast-radius list and one to § Map (R18)
#
# Each prints what is wrong and exits 1, or exits 0 in silence.

import re
import subprocess
import sys

P1 = {"src/mp.hpp", "src/mp.cpp", "src/math.cpp", "tests/vectors/generate.py", "tests/vectors/jacobi.txt",
      "tests/vector_reader.hpp", "tests/vector_reader.cpp", "tests/check_mp.cpp", "tests/check_result.cpp",
      "tests/check_math.cpp", "doc/reference-arithmetic.md", "doc/references.md", "doc/README.md",
      "doc/lucas-lehmer.md"}
P2 = P1 | {"src/lucas.hpp", "src/lucas.cpp", "src/math.hpp", "src/result.hpp", "src/result.cpp",
           "tests/check_lucas.cpp", "tests/ll.sh", "tests/cli.sh", "tests/engine_support.hpp",
           "tests/engine_support.cpp", "Makefile", "AGENTS.md"}
P3 = P2 | {"ci/engine-gate.sh", "doc/prp-and-gerbicz.md"}
ALLOWED = {1: P1, 2: P2, 3: P3, 4: P3}

LABELS = {
    1: {"tests/check_mp.cpp": ["the Jacobi symbol matches the vectors"],
        "tests/check_result.cpp": ["run_in_order stops at the first verdict or error"]},
    2: {"tests/check_lucas.cpp": [
        "the test reports the known answers at two given shifts",
        "the test chooses a fresh shift each run",
        "the test checks the Jacobi symbol at intervals and on the final residue",
        "the test catches degenerate states",
        "the test confirms a zero residue before it reports prime",
        "the test refuses exponents it cannot run",
        "the result of the test names its run in both views",
        "the test recovers from a failed check, up to its limit",
        "the test replays a refused squaring once",
        "the test moves to a larger length when a refusal reproduces",
        "the test counts and logs large round-off distances",
        "the test logs its progress"]},
}

# Declarations, whitespace aside, that must appear in the file
DECLS = {
    1: {"src/mp.hpp": ["int jacobi(const BigInt& a, const BigInt& n);"]},
    2: {"src/lucas.hpp": [
            "static const std::uint64_t GOOD_STATES =", "static const std::uint64_t CHECK_FAILURES =",
            "static const std::uint64_t LENGTH_RETRIES =", "static const double PROGRESS_SECONDS =",
            "static const std::uint64_t DISTANCES_LOGGED =",
            "struct Step {", "std::uint64_t iteration;", "std::uint64_t attempt;", "std::uint64_t length;", "std::uint64_t shift;",
            "struct Options {", "std::optional<std::uint64_t> shift {};", "std::optional<std::uint64_t> length {};",
            "double progress_seconds = PROGRESS_SECONDS;",
            "std::function<void(const Step&)> refuse {};",
            "std::function<std::optional<BigInt>(const Step&, const BigInt&)> corrupt {};",
            "std::function<double(const Step&, double)> distance {};",
            "std::uint64_t largest_exponent();",
            "BigInt unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p);",
            "int jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p);",
            "bool degenerate(const BigInt& s, std::uint64_t p, bool last);",
            "bool confirms_prime(const BigInt& before_last, std::uint64_t p);",
            "Result test(std::uint64_t p, const Options& options = {});"],
        "src/result.hpp": [
            "static const int RECORD_VERSION = 2;", "struct RunReport {",
            "std::vector<std::uint64_t> lengths {};", "std::uint64_t shift = 0;", "std::uint64_t jacobi_checks = 0;",
            "std::uint64_t jacobi_failures = 0;", "std::uint64_t degenerate_failures = 0;",
            "std::uint64_t confirmation_failures = 0;", "std::uint64_t transient_faults = 0;",
            "std::uint64_t length_retries = 0;", "std::uint64_t large_distances = 0;", "double largest_distance = 0;",
            "static Result prime_by_residue(Worktype worktype, long unsigned p, const RunReport& run);",
            "static Result composite_by_residue(Worktype worktype, long unsigned p, std::uint64_t res64, const RunReport& run);",
            "static Result error(Worktype worktype, long unsigned p, const std::string& reason, const RunReport& run);",
            "const std::optional<RunReport>& run() const"]},
}

# Definitions: the whole first line, unindented, exactly once in the file
DEFS = {
    1: {"src/mp.cpp": ["int jacobi(const BigInt& a, const BigInt& n) {"],
        "src/math.cpp": ["Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed) {",
                         "Result test_lucas_lehmer(const long unsigned p) {"],
        "src/ibdwt.cpp": ["double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {"]},
    2: {"src/lucas.cpp": ["std::uint64_t lucas::largest_exponent() {",
                          "BigInt lucas::unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p) {",
                          "int lucas::jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p) {",
                          "bool lucas::degenerate(const BigInt& s, std::uint64_t p, bool last) {",
                          "bool lucas::confirms_prime(const BigInt& before_last, std::uint64_t p) {",
                          "Result lucas::test(std::uint64_t p, const Options& options) {"]},
}

# The lines tests/cli.sh reads from AGENTS.md, and the profile's verify row
KEPT = ["- `src/result.hpp`, `src/result.cpp`", "- `src/main.cpp`",
        "| verify | `make check`: the checks and the smoke test, both always run |"]
R17_NAMES = ["`src/ibdwt.*`", "`src/fft.*`", "`src/lengths.*`", "`src/lengths.txt`", "`src/measure.cpp`", "`IEEE`",
             "make check-engine", "ci/engine-gate.sh"]


HB_START = "### High-blast-radius paths (human gate)"
HB_END = "### 1. Verdicts are earned (`src/math.*` and every test driver) [human gate]"


def hb_section(text):
    lines = text.split("\n")
    if HB_START not in lines or HB_END not in lines:
        return []
    return lines[lines.index(HB_START):lines.index(HB_END)]


def map_section(text):
    return text[text.find("## Map"):text.find("## Factory")].split("\n")


def added(commit, paths, bad):
    def show(rev):
        return subprocess.run(["git", "show", "%s:AGENTS.md" % rev], capture_output=True, text=True).stdout
    now, before = show(commit), show(commit + "^")
    for p in paths:
        tick = "`%s`" % p
        hb_new = [l for l in hb_section(now) if l.startswith("- ") and tick in l and l not in hb_section(before)]
        if not hb_new:
            bad.append("%s's AGENTS.md adds no high-blast-radius line naming %s (R18)" % (commit[:7], tick))
        mp_before = map_section(before)
        mp_new = [l for l in map_section(now) if tick in l and l not in mp_before]
        if not mp_new:
            bad.append("%s's AGENTS.md adds no § Map line naming %s (R18)" % (commit[:7], tick))


def upto(table, phase):
    out = {}
    for ph in sorted(table):
        if ph <= phase:
            for f, items in table[ph].items():
                out.setdefault(f, []).extend(items)
    return out


def squash(s):
    return re.sub(r"\s+", " ", s).strip()


def read(path):
    try:
        return open(path, errors="replace").read()
    except OSError:
        return None


def changes(phase, bad):
    allowed = ALLOWED[min(phase, 4)]
    for p in sys.stdin.read().split():
        if p.startswith("spec/"):
            continue
        if p.startswith("issues/") or p == "ROADMAP.md":
            if phase < 4:
                bad.append("%s changed before the last phase (the deferral ledger is the last phase's)" % p)
            continue
        if p not in allowed:
            bad.append("%s changed: not a path phase %d may change (PLAN § 2.1)" % (p, phase))


def interface(phase, bad):
    for f, decls in upto(DECLS, phase).items():
        text = read(f)
        if text is None:
            bad.append("%s is missing" % f)
            continue
        flat = squash(text)
        for d in decls:
            if squash(d) not in flat:
                bad.append("%s does not declare: %s" % (f, d))
    for f, defs in upto(DEFS, phase).items():
        text = read(f)
        if text is None:
            bad.append("%s is missing" % f)
            continue
        lines = text.split("\n")
        for d in defs:
            n = lines.count(d)
            if n != 1:
                bad.append("%s holds the first line '%s' %d times, want once, unindented" % (f, d, n))
    if phase >= 2:
        body = read("src/lucas.cpp") or ""
        test = body[body.find("Result lucas::test("):]
        for name in ("jacobi_of(", "degenerate(", "confirms_prime(", "unshifted(", "square_subtract("):
            if name not in test:
                bad.append("lucas::test does not call %s (the gates' mutants wrap it)" % name)
        math = read("src/math.cpp") or ""
        if "lucas::test(p)" not in math[math.find("Result test_lucas_lehmer("):][:600]:
            bad.append("test_lucas_lehmer does not run lucas::test(p) with the default options")


def labels(phase, bad):
    for f, names in upto(LABELS, phase).items():
        text = read(f)
        if text is None:
            bad.append("%s is missing" % f)
            continue
        table = text[text.rfind("check_by_name {"):]
        for n in names:
            k = table.count('{"%s",' % n)
            if k != 1:
                bad.append("%s's table registers '%s' %d times, want once" % (f, n, k))


def agents(phase, base_path, bad):
    base = open(base_path).read().split("\n")
    text = read("AGENTS.md")
    lines = text.split("\n")
    hb = base[base.index("### High-blast-radius paths (human gate)"):]
    hb = [l for l in hb[:hb.index("### 1. Verdicts are earned (`src/math.*` and every test driver) [human gate]")] if l.startswith("- ")]
    for l in hb + KEPT:
        if l not in lines:
            bad.append("AGENTS.md lost the line: %s" % l)
    if phase >= 2:
        if not any(l.startswith("- `src/lucas.hpp`, `src/lucas.cpp`") for l in hb_section(text)):
            bad.append("AGENTS.md's high-blast-radius list has no line for `src/lucas.hpp`, `src/lucas.cpp` (R18)")
        mp = text[text.find("## Map"):text.find("## Factory")]
        for n in ("`src/lucas.cpp`", "`src/lucas.hpp`", "check_lucas.cpp", "ll.sh"):
            if n not in mp:
                bad.append("AGENTS.md § Map does not name %s (R18)" % n)
    if phase >= 3:
        br = text[text.find("## Build and run"):text.find("## Map")]
        mp = text[text.find("## Map"):text.find("## Factory")]
        for n in ("make check-long", "ci/engine-gate.sh"):
            if n not in br:
                bad.append("AGENTS.md § Build and run does not name %s (R18)" % n)
        for n in ("check-long", "ci/engine-gate.sh"):
            if n not in mp:
                bad.append("AGENTS.md § Map does not name %s (R18)" % n)
        rows = [l for l in lines if l.startswith("| landing gate |")]
        if len(rows) != 1:
            bad.append("the profile has %d `landing gate` rows, want one (R17)" % len(rows))
        else:
            for n in R17_NAMES:
                if n not in rows[0]:
                    bad.append("the landing gate row does not name %s (R17)" % n)


def recipe(text, target):
    m = re.search(r"^%s:[^\n]*\n((?:\t[^\n]*\n)+)" % re.escape(target), text, re.M)
    return m.group(1) if m else None


def makefile(phase, base_path, bad):
    base, text = open(base_path).read(), read("Makefile")
    for var in ("CXXFLAGS", "IEEE", "CPPFLAGS", "BOUNDS", "SETTINGS", "CHANGED"):
        pat = re.compile(r"^%s\s*:=.*$" % var, re.M)
        if pat.findall(base) != pat.findall(text):
            bad.append("the Makefile's %s differs from bcb6fdf's" % var)
    if phase >= 2:
        mods = re.search(r"^MODULES\s*:=(.*)$", text, re.M)
        if not mods or "lucas" not in mods.group(1).split():
            bad.append("MODULES does not hold lucas")
        for target in ("check", "check-engine"):
            r = recipe(text, target)
            if r is None or "tests/ll.sh $(EXE)" not in r or "tests/cli.sh $(EXE)" not in r or "$(CHECK)" not in r or "|| true" in r:
                bad.append("make %s does not run bin/check, tests/cli.sh and tests/ll.sh (PLAN § 2.6)" % target)
        r = recipe(text, "check-engine")
        if r is None or "$(CHECK) engine" not in r:
            bad.append("make check-engine no longer runs the engine tier")
    if phase >= 3:
        r = recipe(text, "check-long")
        if r is None or "tests/ll.sh $(EXE) long" not in r:
            bad.append("no check-long target running tests/ll.sh $(EXE) long (R16)")
        phony = re.search(r"^\.PHONY:(.*)$", text, re.M)
        if not phony or "check-long" not in phony.group(1).split():
            bad.append("check-long is not .PHONY")


def source(phase, bad):
    tracked = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "src", "tests", "ci"],
                             capture_output=True, text=True).stdout.split()
    new = subprocess.run(["git", "diff", "--name-only", "--diff-filter=A", "bcb6fdf", "--", "src", "tests", "ci"],
                         capture_output=True, text=True).stdout.split()
    new += subprocess.run(["git", "ls-files", "--others", "--exclude-standard", "src", "tests", "ci"],
                          capture_output=True, text=True).stdout.split()
    for f in sorted(set(new)):
        text = read(f) or ""
        head = text.split("\n")[:4]
        if f.endswith((".cpp", ".hpp")):
            if head[:2] != ["// SPDX-FileCopyrightText: 2026 Geoffrey Lentner", "// SPDX-License-Identifier: Apache-2.0"]:
                bad.append("%s does not open with the SPDX block" % f)
        elif f.endswith(".sh"):
            if "# SPDX-License-Identifier: Apache-2.0" not in head + text.split("\n")[1:4]:
                bad.append("%s carries no SPDX licence line" % f)
    for f in tracked:
        if not f.endswith((".cpp", ".hpp", ".sh")):
            continue
        text = read(f) or ""
        added = subprocess.run(["git", "diff", "-U0", "bcb6fdf", "--", f], capture_output=True, text=True).stdout
        added = [l[1:] for l in added.split("\n") if l.startswith("+") and not l.startswith("+++")]
        if f in new:
            added = text.split("\n")
        for l in added:
            if re.search(r"\b[RP][0-9]{1,2}\b", l) and not re.search(r"\bP-1\b", l):
                bad.append("%s: a spec id in source (AGENTS.md § Conventions): %s" % (f, l.strip()[:100]))
    if phase >= 2:
        lucas = read("src/lucas.cpp") or ""
        for n in ("reference.hpp", "reference::", "known.hpp", "known::"):
            if n in lucas:
                bad.append("src/lucas.cpp uses %s: a verdict never comes from the reference or the tables" % n)
        for f in tracked:
            if not f.startswith("src/") or not f.endswith((".cpp", ".hpp")):
                continue
            text = read(f) or ""
            base = subprocess.run(["git", "show", "bcb6fdf:" + f], capture_output=True, text=True).stdout
            if text.count("getenv") > base.count("getenv"):
                bad.append("%s reads the environment more than at bcb6fdf: no variable may reach an injection (R16)" % f)
            if f == "src/lucas.hpp" or ("lucas.hpp" not in text and f != "src/lucas.cpp"):
                continue
            for m in re.finditer(r"\.(refuse|corrupt|distance)\s*=(?!=)", text):
                bad.append("%s sets the test-only hook `%s`: only the checks may (R16)" % (f, m.group(1)))
        for n in ("getenv", "environ", "fstream", "fopen", "argv", "std::cin"):
            if n in lucas:
                bad.append("src/lucas.cpp uses %s: nothing outside the checks may reach the run's options (R16)" % n)
        for f in tracked:
            if f.startswith("src/") and f.endswith((".cpp", ".hpp")) and f not in ("src/lucas.cpp", "src/lucas.hpp"):
                if re.search(r"\blucas::Options\b|using namespace lucas", read(f) or ""):
                    bad.append("%s builds lucas::Options: bin/mps runs the test with its defaults only (R16)" % f)
        for f in ("src/main.cpp", "src/cli.cpp", "src/cli.hpp"):
            if "lucas::" in (read(f) or "") or "lucas.hpp" in (read(f) or ""):
                bad.append("%s reaches src/lucas.hpp directly: bin/mps runs the test only through test_lucas_lehmer" % f)


def main(argv):
    mode = argv[1]
    phase = int(argv[2]) if mode != "added" else 0
    bad = []
    if mode == "changes":
        changes(phase, bad)
    elif mode == "interface":
        interface(phase, bad)
    elif mode == "labels":
        labels(phase, bad)
    elif mode == "agents":
        agents(phase, argv[3], bad)
    elif mode == "makefile":
        makefile(phase, argv[3], bad)
    elif mode == "source":
        source(phase, bad)
    elif mode == "added":
        added(argv[2], argv[3:], bad)
    else:
        bad.append("unknown mode " + mode)
    for b in bad:
        print("files: " + b, file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
