# Review cycle 2, F5 (a cycle record, not a standing gate): no statement of src/lucas.cpp that feeds a
# value the test reports can change while make check stays green. The values: the progress line's
# (the iteration and the run's last, the time per iteration since the last line, the time to go,
# the largest distance since the last line, and the variables and resets behind them), the run
# report's fields and counters, the logged and folded distances, and the transient, failure and
# retry counts.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py sweep SCRATCH [COMPILER...]
#       Every mutant below, each under make check with clang++ and with g++-16, in two copies of
#       the tree under SCRATCH (a directory outside the repository); writes the record,
#       F5-mutants.txt beside this script. About 30 minutes on an M2 Mac. Naming compilers runs
#       those alone and writes a partial record under SCRATCH instead.
#   python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py check [ROOT]
#       The checker: the record is of this tree (the digests of src/lucas.cpp, the files whose
#       checks kill the mutants and the mutant table below are the record's), every mutant in it
#       was killed under both compilers or is listed as equivalent with its reason; and, live, in
#       a copy of the tree in a temporary directory, make check under clang++ is green as it is and
#       red under each of the three mutants review cycle 2 found alive (the line's largest distance
#       not reset, the line's time not reset, the time to go from the whole run). About 2 minutes.
#
# The progress line's times come from the system clock, which no option sets: the checks bound
# them by sleeps in a hook and pin exactly what does not depend on the clock. A mutant that only
# moves a time below what a line prints, and so changes the text only by chance, is listed as
# equivalent with "not observable without a hook on the clock" and why.
#
# A mutant is one exact edit of src/lucas.cpp: its text must occur exactly once. It is killed when
# make check exits other than 0 or runs past its time limit. Before each make the objects and
# binaries built from src/lucas.cpp are deleted, so a mutant is always compiled, whatever the
# timestamps; a make check past its limit is killed with its process group. Prints what is wrong
# and exits 1, or prints one line and exits 0.

import hashlib
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
RECORD = os.path.join(HERE, "F5-mutants.txt")
MAKE = "/usr/bin/make"
COMPILERS = ["clang++", "g++-16"]
LIMIT = 150          # a mutant's make check, which takes 6 to 25 s on an M2 Mac
BUILD_LIMIT = 900    # a make check that builds everything
DIGESTED = ["src/lucas.cpp", "src/lucas.hpp", "tests/check_lucas.cpp", "tests/cli.sh", "tests/ll.sh"]
REBUILT = ["lib/lucas.o", "lib/check/lucas.o", "bin/mps", "bin/check"]
LIVE = ["line-largest-unreset", "line-time-unreset", "to-go-whole-run"]

CLOCK = "not observable without a hook on the clock: "
RETRY = ("\t\t\t\t\tif (run.length_retries == LENGTH_RETRIES)\n"
         "\t\t\t\t\t\treturn Result::error(WORK_LL, p, what + \"; the run has moved to a larger length \" +\n"
         "\t\t\t\t\t\t                     std::to_string(LENGTH_RETRIES) + \" times, the limit\", run);\n")
CHECKED = ("\t\t\tstd::string failed;\n\t\t\tif (degenerate(s, p, i == last)) {\n\t\t\t\trun.degenerate_failures++;\n"
           "\t\t\t\tfailed = \"the degenerate-state check\";\n\t\t\t} else {\n\t\t\t\trun.jacobi_checks++;\n")
HOOKED = ("\t\t\tif (options.distance)\n\t\t\t\tdistance = options.distance(step, distance);\n"
          "\t\t\trun.largest_distance = std::max(run.largest_distance, distance);\n")
REPLAYED = "\t\t\t\t\tdistance = ibdwt::square_subtract(*engine, x, bit);\n\t\t\t\t\trun.transient_faults++;\n"

# (id, value it feeds, old text, new text, why it is equivalent or None)
MUTANTS = [
    # The progress line: its variables and their resets
    ("line-largest-unreset", "progress line", "\t\t\t\tline_largest = 0;\n", "", None),
    ("line-squarings-unreset", "progress line", "\t\t\t\tline_squarings = 0;\n", "", None),
    ("line-time-unreset", "progress line", "\t\t\t\tline_time = now;\n", "", None),
    ("line-time-from-epoch", "progress line", "Clock::time_point line_time = Clock::now();", "Clock::time_point line_time {};", None),
    ("line-squarings-uncounted", "progress line", "\t\t\tline_squarings++;\n", "", None),
    ("line-largest-unset", "progress line", "\t\t\tline_largest = std::max(line_largest, distance);\n", "", None),
    ("line-largest-from-run", "progress line", "line_largest = std::max(line_largest, distance);",
     "line_largest = std::max(run.largest_distance, distance);", None),
    ("line-shows-last-distance", "progress line", "fixed(line_largest, 4)", "fixed(distance, 4)", None),
    ("progress-iteration-before", "progress line", "logging::info(name, \"iteration \", i, \" of \"", "logging::info(name, \"iteration \", i - 1, \" of \"", None),
    ("progress-total-p", "progress line", "\" of \", last, \", \"", "\" of \", p, \", \"", None),
    ("progress-past-interval", "progress line", "if (seconds >= options.progress_seconds)", "if (seconds > options.progress_seconds)",
     CLOCK + "the two differ only when the seconds since the last line equal the interval exactly; at an interval of 0 an "
     "iteration spans many ticks of the steady clock, so the seconds are never 0, and at any other interval they are a "
     "measured double that no check can make equal to it"),
    # The time per iteration
    ("each-undivided", "progress line", "const double each = seconds / double(line_squarings);", "const double each = seconds;", None),
    ("each-one-more-squaring", "progress line", "seconds / double(line_squarings)", "seconds / double(line_squarings + 1)", None),
    ("each-in-microseconds", "progress line", "fixed(each * 1e3, 3)", "fixed(each * 1e6, 3)", None),
    # The time to go
    ("to-go-whole-run", "progress line", "duration_text(each * double(last - i))", "duration_text(each * double(last))", None),
    ("to-go-from-done", "progress line", "duration_text(each * double(last - i))", "duration_text(each * double(i))", None),
    ("to-go-one-more-iteration", "progress line", "duration_text(each * double(last - i))", "duration_text(each * double(last - i + 1))",
     CLOCK + "it adds one iteration's time to the time to go, which the line prints in whole seconds, so the text changes "
     "only when that crosses a rounding boundary; the time per iteration is measured, so no check can place it there"),
    ("to-go-truncated", "progress line", "static_cast<unsigned long long>(seconds + 0.5)", "static_cast<unsigned long long>(seconds)",
     CLOCK + "it lowers the printed time to go by one second only when the measured time's fraction is a half or more, "
     "which no check can choose; every line's time to go is checked to within half a second of its time per iteration "
     "times the iterations left, which this mutant keeps within one"),
    ("to-go-minutes-unwrapped", "progress line", "total / 3600, total / 60 % 60, total % 60", "total / 3600, total / 60, total % 60", None),
    ("to-go-seconds-unwrapped", "progress line", "total / 3600, total / 60 % 60, total % 60", "total / 3600, total / 60 % 60, total", None),
    ("to-go-hours-as-minutes", "progress line", "total / 3600, total / 60 % 60, total % 60", "total / 60, total / 60 % 60, total % 60", None),
    # The run report's fields
    ("shift-drawn-zero", "run report", "*options.shift % p : random_shift(p)", "*options.shift % p : 0", None),
    ("shift-drawn-past-p", "run report", "uniform_int_distribution<std::uint64_t>(0, p - 1)", "uniform_int_distribution<std::uint64_t>(0, p)", None),
    ("shift-drawn-never-0", "run report", "uniform_int_distribution<std::uint64_t>(0, p - 1)", "uniform_int_distribution<std::uint64_t>(1, p - 1)", None),
    ("length-option-ignored", "run report", "options.length ? *options.length : ibdwt::length_for(p)", "ibdwt::length_for(p)", None),
    ("lengths-first-unrecorded", "run report", "\trun.lengths.push_back(length);\n\n\tconst std::uint64_t last", "\n\tconst std::uint64_t last", None),
    ("lengths-move-unrecorded", "run report", "\t\t\t\t\trun.lengths.push_back(length);\n", "", None),
    ("largest-unset", "run report", "\t\t\trun.largest_distance = std::max(run.largest_distance, distance);\n", "", None),
    ("largest-from-line", "run report", "run.largest_distance = std::max(run.largest_distance, distance);",
     "run.largest_distance = std::max(line_largest, distance);", None),
    ("largest-before-hook", "run report", HOOKED,
     "\t\t\trun.largest_distance = std::max(run.largest_distance, distance);\n\t\t\tif (options.distance)\n"
     "\t\t\t\tdistance = options.distance(step, distance);\n", None),
    ("jacobi-checks-uncounted", "run report", "\t\t\t\trun.jacobi_checks++;\n", "", None),
    ("jacobi-checks-count-degenerate", "run report", CHECKED,
     "\t\t\tstd::string failed;\n\t\t\trun.jacobi_checks++;\n\t\t\tif (degenerate(s, p, i == last)) {\n\t\t\t\trun.degenerate_failures++;\n"
     "\t\t\t\tfailed = \"the degenerate-state check\";\n\t\t\t} else {\n", None),
    ("jacobi-failures-uncounted", "run report", "\t\t\t\t\trun.jacobi_failures++;\n", "", None),
    ("degenerate-uncounted", "run report", "\t\t\t\trun.degenerate_failures++;\n", "", None),
    ("confirmation-uncounted", "run report", "\t\t\t\t\trun.confirmation_failures++;\n", "", None),
    # The logged and folded distances
    ("large-uncounted", "distances", "\t\t\t\trun.large_distances++;\n", "", None),
    ("large-at-sizing", "distances", "if (distance > ibdwt::SIZING_DISTANCE)", "if (distance >= ibdwt::SIZING_DISTANCE)", None),
    ("logged-one-more", "distances", "if (run.large_distances < DISTANCES_LOGGED)", "if (run.large_distances <= DISTANCES_LOGGED)", None),
    ("logged-all", "distances", "if (run.large_distances < DISTANCES_LOGGED)", "if (true)", None),
    ("logged-next-iteration", "distances", "\"iteration \", i, \" at length \", length", "\"iteration \", i + 1, \" at length \", length", None),
    ("logged-exponent-as-length", "distances", "\" at length \", length, \" has round-off distance \"", "\" at length \", p, \" has round-off distance \"", None),
    ("logged-run-largest", "distances", "fixed(distance, 4), \", past \"", "fixed(run.largest_distance, 4), \", past \"", None),
    ("folded-all", "distances", "run.large_distances - DISTANCES_LOGGED,", "run.large_distances,", None),
    ("folded-at-equal", "distances", "if (run.large_distances > DISTANCES_LOGGED)", "if (run.large_distances >= DISTANCES_LOGGED)", None),
    ("folded-off", "distances", "if (run.large_distances > DISTANCES_LOGGED)", "if (false)", None),
    # The transient, failure and retry counts
    ("transient-uncounted", "counts", "\t\t\t\t\trun.transient_faults++;\n", "", None),
    ("transient-before-replay", "counts", REPLAYED,
     "\t\t\t\t\trun.transient_faults++;\n\t\t\t\t\tdistance = ibdwt::square_subtract(*engine, x, bit);\n",
     "not observable through the hooks: the refuse hook throws before the replay's squaring, as the engine's refusal "
     "leaves the residue as it was, so an injected refusal of the replay skips the count in either order; only the "
     "engine's own refusal of a replay tells them apart, and the engine refuses only past its threshold at a length the "
     "table lets hold p, which no option reaches (lucas::test builds it with the default threshold)"),
    ("transient-counted-when-reproduced", "counts", "} catch (roundoff_error const& again) {\n",
     "} catch (roundoff_error const& again) {\n\t\t\t\t\trun.transient_faults++;\n", None),
    ("retries-uncounted", "counts", "\t\t\t\t\trun.length_retries++;\n", "", None),
    ("retries-counted-before-limit", "counts", RETRY + "\t\t\t\t\trun.length_retries++;\n", "\t\t\t\t\trun.length_retries++;\n" + RETRY, None),
    ("retries-limit-early", "counts", "if (run.length_retries == LENGTH_RETRIES)", "if (run.length_retries + 1 == LENGTH_RETRIES)", None),
    ("failures-uncounted", "counts", "\t\t\tfailures++;\n", "\n", None),
    ("failures-reset-at-good-state", "counts", "\t\t\t\tgood = {i, k, r};\n", "\t\t\t\tgood = {i, k, r};\n\t\t\t\tfailures = 0;\n", None),
    ("failures-limit-early", "counts", "if (failures > CHECK_FAILURES)", "if (failures >= CHECK_FAILURES)", None),
    ("failures-count-in-reason", "counts", "std::to_string(failures) + \" failed checks in the run", "std::to_string(CHECK_FAILURES) + \" failed checks in the run", None),
]


def digest(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def table_digest():
    return hashlib.sha256(repr(MUTANTS).encode()).hexdigest()


def copy_tree(root, dest):
    listed = subprocess.run(["git", "-C", root, "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                            capture_output=True, check=True).stdout.split(b"\0")
    for rel in (r.decode() for r in listed if r):
        src = os.path.join(root, rel)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(os.path.join(dest, rel)), exist_ok=True)
            shutil.copy2(src, os.path.join(dest, rel))


def make_check(tree, cxx, limit=LIMIT):
    for rel in REBUILT:
        if os.path.exists(os.path.join(tree, rel)):
            os.remove(os.path.join(tree, rel))
    start = time.time()
    run = subprocess.Popen([MAKE, "-C", tree, "CXX=" + cxx, "check"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, start_new_session=True)
    try:
        out, _ = run.communicate(timeout=limit)
        code = run.returncode
    except subprocess.TimeoutExpired:
        os.killpg(run.pid, signal.SIGKILL)
        out, _ = run.communicate()
        code = "timeout"
    seconds = time.time() - start
    sums = [l for l in out.split("\n") if l.split(":")[0] in ("check", "cli", "ll") and " passed, " in l]
    failed = [l for l in sums if not l.endswith(" 0 failed")]
    return code, sums, failed, seconds, out


def mutate(text, old, new):
    if text.count(old) != 1:
        return None
    return text.replace(old, new)


def verdict(code, sums, failed):
    if code == 0:
        return "survived"
    if code == "timeout":
        return "killed (timed out)"
    if failed:
        return "killed (%s)" % "; ".join(failed)
    if len(sums) < 3:
        return "killed (exit %s, %d of 3 summaries: build failed or a step crashed)" % (code, len(sums))
    return "killed (exit %s)" % code


def sweep(scratch, compilers):
    record = RECORD if compilers == COMPILERS else os.path.join(os.path.abspath(scratch), "F5-mutants.partial.txt")
    root = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
    scratch = os.path.abspath(scratch)
    if scratch.startswith(root + os.sep) or scratch == root:
        sys.exit("F5: the scratch directory must be outside the repository")
    original = open(os.path.join(root, "src/lucas.cpp")).read()
    for mid, _, old, new, _ in MUTANTS:
        if mutate(original, old, new) is None:
            sys.exit("F5: mutant %s: its text occurs %d times in src/lucas.cpp, want once" % (mid, original.count(old)))
    trees = {}
    for cxx in compilers:
        tree = os.path.join(scratch, "tree-" + cxx)
        if os.path.exists(tree):
            shutil.rmtree(tree)
        copy_tree(root, tree)
        code, sums, failed, seconds, out = make_check(tree, cxx, BUILD_LIMIT)
        if code != 0:
            print(out[-3000:])
            sys.exit("F5: make check under %s is red without a mutant" % cxx)
        print("baseline %s: green in %.0f s (%s)" % (cxx, seconds, "; ".join(sums)), flush=True)
        trees[cxx] = tree
    rows = []
    for mid, value, old, new, equivalent in MUTANTS:
        results = []
        for cxx in compilers:
            lucas = os.path.join(trees[cxx], "src/lucas.cpp")
            open(lucas, "w").write(mutate(original, old, new))
            code, sums, failed, seconds, _ = make_check(trees[cxx], cxx)
            open(lucas, "w").write(original)
            results.append("%s: %s, %.0f s" % (cxx, verdict(code, sums, failed), seconds))
        row = "%s\t%s\t%s" % (mid, value, "\t".join(results))
        if equivalent:
            row += "\tequivalent: " + equivalent
        rows.append(row)
        print(row, flush=True)
    for cxx in compilers:
        code, _, _, _, _ = make_check(trees[cxx], cxx)
        if code != 0:
            sys.exit("F5: make check under %s is red after the sweep restored src/lucas.cpp" % cxx)
    killed = {cxx: sum(1 for r in rows if ("%s: killed" % cxx) in r) for cxx in compilers}
    survivors = [r.split("\t")[0] for r in rows if "survived" in r and "\tequivalent: " not in r]
    equivalents = [r.split("\t")[0] for r in rows if "\tequivalent: " in r]
    with open(record, "w") as f:
        f.write("# F5's mutant sweep over the statements of src/lucas.cpp that feed a reported value (review cycle 2), written by\n")
        f.write("#   python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py sweep SCRATCH\n")
        f.write("# Each mutant under make check with clang++ and g++-16; killed = make check exited other than 0.\n")
        f.write("# Columns: id, the value it feeds, clang++ result, g++-16 result, and for an equivalent mutant why.\n")
        for rel in DIGESTED:
            f.write("sha256 %s %s\n" % (rel, digest(os.path.join(root, rel))))
        f.write("sha256 mutant-table %s\n" % table_digest())
        for r in rows:
            f.write(r + "\n")
        f.write("# %d mutants: %s; %d equivalent (%s); %d not equivalent survived (%s)\n" %
                (len(rows), ", ".join("%d killed under %s" % (killed[c], c) for c in compilers), len(equivalents),
                 ", ".join(equivalents) or "none", len(survivors), ", ".join(survivors) or "none"))
    print("F5: sweep written to %s: %d mutants, %s, %d equivalent, %d survivors %s" %
          (record, len(rows), ", ".join("%d killed under %s" % (killed[c], c) for c in compilers),
           len(equivalents), len(survivors), survivors))
    sys.exit(1 if survivors else 0)


def check(root):
    root = os.path.abspath(root)
    bad = []
    if not os.path.exists(RECORD):
        bad.append("no record %s: run the sweep" % os.path.basename(RECORD))
        lines = []
    else:
        lines = open(RECORD).read().split("\n")
    sums = {l.split()[1]: l.split()[2] for l in lines if l.startswith("sha256 ")}
    for rel in DIGESTED:
        if lines and sums.get(rel) != digest(os.path.join(root, rel)):
            bad.append("the record's %s is not this tree's: the sweep is stale, run it again" % rel)
    if lines and sums.get("mutant-table") != table_digest():
        bad.append("the record's mutant table is not this script's: the sweep is stale, run it again")
    rows = {l.split("\t")[0]: l.split("\t") for l in lines if l and not l.startswith(("#", "sha256 "))}
    for mid, _, _, _, equivalent in MUTANTS:
        row = rows.get(mid)
        if lines and row is None:
            bad.append("mutant %s is not in the record" % mid)
            continue
        if row is None:
            continue
        both = all(any(f.startswith("%s: killed" % c) for f in row) for c in COMPILERS)
        if not both and not (equivalent and any(f.startswith("equivalent: ") for f in row)):
            bad.append("mutant %s survived: %s" % (mid, " | ".join(row[2:])))

    # Live: the mutants review cycle 2 found alive, against a green tree
    live = []
    work = tempfile.mkdtemp(prefix="mps-F5.")
    try:
        tree = os.path.join(work, "tree")
        copy_tree(root, tree)
        code, s, failed, seconds, out = make_check(tree, "clang++", BUILD_LIMIT)
        if code != 0:
            bad.append("make check under clang++ is red without a mutant: %s" % "; ".join(s))
        else:
            lucas = os.path.join(tree, "src/lucas.cpp")
            text = open(lucas).read()
            for mid in LIVE:
                _, _, old, new, _ = [m for m in MUTANTS if m[0] == mid][0]
                mutated = mutate(text, old, new)
                if mutated is None:
                    bad.append("mutant %s: its text does not occur exactly once in src/lucas.cpp" % mid)
                    continue
                open(lucas, "w").write(mutated)
                code, s, failed, seconds, out = make_check(tree, "clang++")
                open(lucas, "w").write(text)
                if code == 0:
                    bad.append("live: make check under clang++ stays green under %s (%s)" % (mid, "; ".join(s)))
                else:
                    live.append("%s %s" % (mid, verdict(code, s, failed)))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for b in bad:
        print("F5: " + b)
    if bad:
        sys.exit(1)
    print("F5: the sweep's %d mutants are this tree's, each killed under both compilers or equivalent; live under clang++, %s" %
          (len(MUTANTS), "; ".join(live)))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "sweep":
        sweep(sys.argv[2], sys.argv[3:] or COMPILERS)
    elif len(sys.argv) >= 2 and sys.argv[1] == "check":
        check(sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", ".."))
    else:
        sys.exit("usage: F5-mutants.py sweep SCRATCH | check [ROOT]")
