# Review cycle 1, F3 (a cycle record, not a standing gate): no decision in src/lucas.cpp can change
# while make check stays green.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py sweep SCRATCH [COMPILER...]
#       Every mutant below, each under make check with clang++ and with g++-16, in two copies of
#       the tree under SCRATCH (a directory outside the repository); writes the record,
#       F3-mutants.txt beside this script. About an hour on an M2 Mac. Naming compilers runs
#       those alone and writes a partial record under SCRATCH instead.
#   python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py check [ROOT]
#       The checker: the record is of this tree (the digests of src/lucas.cpp, the files whose
#       checks kill the mutants and the mutant table below are the record's), every mutant in it
#       was killed under both compilers or is listed as equivalent with its reason; and, live, in
#       a copy of the tree in a temporary directory, make check under clang++ is green as it is and
#       red with a Jacobi symbol of 0 passing the check (the mutant review cycle 1 found alive).
#       About 1 minute.
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
RECORD = os.path.join(HERE, "F3-mutants.txt")
MAKE = "/usr/bin/make"
COMPILERS = ["clang++", "g++-16"]
LIMIT = 150          # a mutant's make check, which takes 6 to 25 s on an M2 Mac
BUILD_LIMIT = 900    # a make check that builds everything
DIGESTED = ["src/lucas.cpp", "src/lucas.hpp", "tests/check_lucas.cpp", "tests/cli.sh", "tests/ll.sh"]
REBUILT = ["lib/lucas.o", "lib/check/lucas.o", "bin/mps", "bin/check"]
LIVE = "jacobi-zero-passes"

# (id, decision, old text, new text, why it is equivalent or None)
MUTANTS = [
    # The refusals and their order
    ("refuse-below-3", "refusals", "if (p < 3 or p % 2 == 0)\n", "if (p < 2 or p % 2 == 0)\n",
     "2 is even, so p < 3 or p even and p < 2 or p even hold for the same exponents"),
    ("refuse-1-runs", "refusals", "if (p < 3 or p % 2 == 0)\n", "if (p < 1 or p % 2 == 0)\n", None),
    ("refuse-even-off", "refusals", "if (p < 3 or p % 2 == 0)\n", "if (p < 3)\n", None),
    ("refuse-past-at-pmax", "refusals", "if (p > largest)\n", "if (p >= largest)\n", None),
    ("refuse-past-from-pmax-2", "refusals", "if (p > largest)\n", "if (p > largest + 1)\n", None),
    ("refuse-past-odd-only", "refusals", "if (p > largest)\n", "if (p > largest and p % 2 == 1)\n", None),
    ("refuse-composite-off", "refusals", "smallest_prime_factor(p) != p)", "smallest_prime_factor(p) == 0)", None),
    ("refuse-composite-not-past", "refusals", "if (p >= 3 and p % 2 == 1 and smallest_prime_factor(p) != p)",
     "if (p >= 3 and p % 2 == 1 and p <= largest and smallest_prime_factor(p) != p)", None),
    ("refuse-composite-of-1", "refusals", "if (p >= 3 and p % 2 == 1 and smallest", "if (p % 2 == 1 and smallest", None),
    ("refuse-composite-of-even", "refusals", "if (p >= 3 and p % 2 == 1 and smallest", "if (p >= 3 and smallest", None),
    ("refuse-first-reason-only", "refusals", 'reason += "; " + reasons[n];', 'reason += "";', None),
    ("refuse-order-reversed", "refusals",
     'std::string reason = reasons.front();\n\t\tfor (std::size_t n = 1; n < reasons.size(); n++)\n\t\t\treason += "; " + reasons[n];',
     'std::string reason = reasons.back();\n\t\tfor (std::size_t n = reasons.size() - 1; n-- > 0;)\n\t\t\treason += "; " + reasons[n];', None),
    ("refuse-after-a-squaring", "refusals", "\tif (not reasons.empty()) {\n",
     "\tif (options.refuse)\n\t\toptions.refuse(Step {1, 0, 0, 0});\n\tif (not reasons.empty()) {\n", None),
    # The schedule
    ("schedule-interval-plus-one", "schedule", "(last + GOOD_STATES - 1) / GOOD_STATES", "(last + GOOD_STATES) / GOOD_STATES",
     "p is odd, so p - 2 is odd and never a multiple of GOOD_STATES (32): (n + 32) / 32 and (n + 31) / 32 are both "
     "floor(n / 32) + 1 for every n that is not a multiple of 32, so the interval, and every checked iteration, is the same"),
    ("schedule-interval-floor", "schedule", "(last + GOOD_STATES - 1) / GOOD_STATES", "(last + GOOD_STATES - 1) / GOOD_STATES + 1", None),
    ("schedule-last-unchecked", "schedule", "if (i % interval != 0 and i != last)", "if (i % interval != 0)", None),
    ("schedule-half", "schedule", "if (i % interval != 0 and i != last)", "if (i % (2 * interval) != 0 and i != last)", None),
    ("schedule-before-last", "schedule", "if (i + 3 == p)", "if (i + 4 == p)", None),
    # The degenerate set
    ("degenerate-zero-never", "degenerate set", "return not last;", "return false;", None),
    ("degenerate-zero-always", "degenerate set", "return not last;", "return true;", None),
    ("degenerate-no-1", "degenerate set", "return s == BigInt(1) or s == BigInt(2) or", "return s == BigInt(2) or", None),
    ("degenerate-no-2", "degenerate set", "return s == BigInt(1) or s == BigInt(2) or", "return s == BigInt(1) or", None),
    ("degenerate-no-minus-1", "degenerate set", " or s == subtract(m, BigInt(1)) or", " or", None),
    ("degenerate-no-minus-2", "degenerate set", " or s == subtract(m, BigInt(2));", ";", None),
    ("degenerate-off", "degenerate set", "if (degenerate(s, p, i == last)) {", "if (false and degenerate(s, p, i == last)) {", None),
    ("degenerate-never-last", "degenerate set", "degenerate(s, p, i == last)", "degenerate(s, p, false)", None),
    # The Jacobi acceptance
    ("jacobi-zero-passes", "Jacobi acceptance", "if (symbol != -1) {", "if (symbol == 1) {", None),
    ("jacobi-one-passes", "Jacobi acceptance", "if (symbol != -1) {", "if (symbol == 0) {", None),
    ("jacobi-off", "Jacobi acceptance", "if (symbol != -1) {", "if (false) {", None),
    ("jacobi-argument-s-minus-1", "Jacobi acceptance", "shift_left(BigInt(1), (k + 1) % p)", "shift_left(BigInt(1), k % p)", None),
    ("jacobi-argument-negated", "Jacobi acceptance", ": subtract(add(r, m), power)", ": subtract(power, r)", None),
    ("jacobi-uncounted", "Jacobi acceptance", "run.jacobi_checks++;", "", None),
    ("jacobi-failures-uncounted", "Jacobi acceptance", "run.jacobi_failures++;", "", None),
    ("jacobi-zero-logged-as-plus-1", "Jacobi acceptance", '(symbol == 1 ? "+1" : "0")', '(symbol == 1 ? "+1" : "+1")', None),
    # The prime confirmation
    ("confirmation-off", "confirmation",
     "} else if (i == last and s.is_zero() and not (before_last and confirms_prime(*before_last, p))) {", "} else if (false) {", None),
    ("confirmation-unread-passes", "confirmation", "not (before_last and confirms_prime(*before_last, p))",
     "(before_last and not confirms_prime(*before_last, p))",
     "s_(p-3) is always read when the last check runs: the loop reaches iteration p - 2 only from p - 3, where it reads "
     "s_(p-3) first, and every return to a good state goes to an iteration at most p - 3 (the last good state before the "
     "end), from which the loop passes p - 3 again; so before_last is engaged and the two conditions agree"),
    ("confirmation-plus-root-only", "confirmation", "return before_last == root or before_last == subtract(m, root);",
     "return before_last == root;", None),
    ("confirmation-minus-root-only", "confirmation", "return before_last == root or before_last == subtract(m, root);",
     "return before_last == subtract(m, root);", None),
    ("confirmation-root-exponent", "confirmation", "shift_left(BigInt(1), (p + 1) / 2)", "shift_left(BigInt(1), (p - 1) / 2)", None),
    ("confirmation-uncounted", "confirmation", "run.confirmation_failures++;", "", None),
    # The failure and retry limits
    ("failures-limit-early", "failure limit", "if (failures > CHECK_FAILURES)", "if (failures >= CHECK_FAILURES)", None),
    ("failures-limit-late", "failure limit", "if (failures > CHECK_FAILURES)", "if (failures > CHECK_FAILURES + 1)", None),
    ("failures-uncounted", "failure limit", "\t\t\tfailures++;\n", "\n", None),
    ("retries-limit-late", "retry limit", "if (run.length_retries == LENGTH_RETRIES)", "if (run.length_retries > LENGTH_RETRIES)", None),
    ("retries-limit-early", "retry limit", "if (run.length_retries == LENGTH_RETRIES)", "if (run.length_retries + 1 == LENGTH_RETRIES)", None),
    ("retries-uncounted", "retry limit", "\t\t\t\t\trun.length_retries++;\n", "\n", None),
    ("replay-unrefused", "retry limit", "step.attempt = runs[i - good.iteration]++;\n\t\t\t\t\tif (options.refuse)\n\t\t\t\t\t\toptions.refuse(step);\n",
     "step.attempt = runs[i - good.iteration]++;\n", None),
    ("replay-uncounted", "retry limit", "run.transient_faults++;", "", None),
    ("retry-same-length", "retry limit", "if (candidate > length and ibdwt::holds(candidate, p))", "if (candidate >= length and ibdwt::holds(candidate, p))", None),
    ("retry-length-not-holding", "retry limit", "if (candidate > length and ibdwt::holds(candidate, p))", "if (candidate > length)", None),
    ("retry-engine-kept", "retry limit", "\t\t\t\t\tengine.emplace(p, length);\n\t\t\t\t\trestore();\n", "\t\t\t\t\trestore();\n", None),
    ("retry-carries-residue", "retry limit", "\t\t\t\t\tengine.emplace(p, length);\n\t\t\t\t\trestore();\n\t\t\t\t\tcontinue;\n",
     "\t\t\t\t\t{ const BigInt carried = ibdwt::from_digits(*engine, x); engine.emplace(p, length); x = ibdwt::to_digits(*engine, carried); }\n"
     "\t\t\t\t\tcontinue;\n", None),
    # Restore and the good state's advance
    ("restore-keeps-iteration", "restore", "\t\t\ti = good.iteration;\n\t\t\tk = good.shift;\n", "\t\t\tk = good.shift;\n", None),
    ("restore-keeps-shift", "restore", "\t\t\tk = good.shift;\n\t\t\tx = ibdwt", "\t\t\tx = ibdwt", None),
    ("restore-keeps-residue", "restore", "\t\t\tx = ibdwt::to_digits(*engine, good.residue);\n\t\t};", "\t\t};", None),
    ("good-state-unadvanced", "good state's advance", "\t\t\t\tgood = {i, k, r};\n", "", None),
    ("good-state-attempts-kept", "good state's advance", "\t\t\t\tstd::fill(runs.begin(), runs.end(), 0);\n", "", None),
    ("good-state-after-failure", "good state's advance", "\t\t\tfailures++;\n", "\t\t\tgood = {i, k, r};\n\t\t\tfailures++;\n", None),
    ("verdict-from-held-residue", "good state's advance", "const BigInt s = unshifted(good.residue, good.shift, p);",
     "const BigInt s = unshifted(ibdwt::from_digits(*engine, x), k, p);",
     "the loop ends only after the check at iteration p - 2 passes, which makes the good state that very residue and shift "
     "(good = {i, k, r} with r read from x), and nothing changes x or k after it, so both read the same value"),
    ("advance-shift-kept", "good state's advance", "\t\t\tk = step.shift;\n", "\n", None),
    ("advance-bit", "good state's advance", "const std::uint64_t bit = (2 * k + 1) % p;", "const std::uint64_t bit = 2 * k % p;", None),
    ("advance-given-shift-unreduced", "good state's advance", "*options.shift % p", "*options.shift", None),
    ("advance-unshift", "good state's advance", "shift_left(r, (p - k % p) % p)", "shift_left(r, k % p)", None),
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
    record = RECORD if compilers == COMPILERS else os.path.join(os.path.abspath(scratch), "F3-mutants.partial.txt")
    root = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
    scratch = os.path.abspath(scratch)
    if scratch.startswith(root + os.sep) or scratch == root:
        sys.exit("F3: the scratch directory must be outside the repository")
    original = open(os.path.join(root, "src/lucas.cpp")).read()
    for mid, _, old, new, _ in MUTANTS:
        if mutate(original, old, new) is None:
            sys.exit("F3: mutant %s: its text occurs %d times in src/lucas.cpp, want once" % (mid, original.count(old)))
    trees = {}
    for cxx in compilers:
        tree = os.path.join(scratch, "tree-" + cxx)
        if os.path.exists(tree):
            shutil.rmtree(tree)
        copy_tree(root, tree)
        code, sums, failed, seconds, out = make_check(tree, cxx, BUILD_LIMIT)
        if code != 0:
            print(out[-3000:])
            sys.exit("F3: make check under %s is red without a mutant" % cxx)
        print("baseline %s: green in %.0f s (%s)" % (cxx, seconds, "; ".join(sums)), flush=True)
        trees[cxx] = tree
    rows = []
    for mid, decision, old, new, equivalent in MUTANTS:
        results = []
        for cxx in compilers:
            lucas = os.path.join(trees[cxx], "src/lucas.cpp")
            open(lucas, "w").write(mutate(original, old, new))
            code, sums, failed, seconds, _ = make_check(trees[cxx], cxx)
            open(lucas, "w").write(original)
            results.append("%s: %s, %.0f s" % (cxx, verdict(code, sums, failed), seconds))
        row = "%s\t%s\t%s" % (mid, decision, "\t".join(results))
        if equivalent:
            row += "\tequivalent: " + equivalent
        rows.append(row)
        print(row, flush=True)
    for cxx in compilers:
        code, _, _, _, _ = make_check(trees[cxx], cxx)
        if code != 0:
            sys.exit("F3: make check under %s is red after the sweep restored src/lucas.cpp" % cxx)
    killed = {cxx: sum(1 for r in rows if ("%s: killed" % cxx) in r) for cxx in compilers}
    survivors = [r.split("\t")[0] for r in rows if "survived" in r and "\tequivalent: " not in r]
    equivalents = [r.split("\t")[0] for r in rows if "\tequivalent: " in r]
    with open(record, "w") as f:
        f.write("# F3's mutant sweep over src/lucas.cpp (review cycle 1), written by\n")
        f.write("#   python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py sweep SCRATCH\n")
        f.write("# Each mutant under make check with clang++ and g++-16; killed = make check exited other than 0.\n")
        f.write("# Columns: id, decision, clang++ result, g++-16 result, and for an equivalent mutant why.\n")
        for rel in DIGESTED:
            f.write("sha256 %s %s\n" % (rel, digest(os.path.join(root, rel))))
        f.write("sha256 mutant-table %s\n" % table_digest())
        for r in rows:
            f.write(r + "\n")
        f.write("# %d mutants: %s; %d equivalent (%s); %d not equivalent survived (%s)\n" %
                (len(rows), ", ".join("%d killed under %s" % (killed[c], c) for c in compilers), len(equivalents),
                 ", ".join(equivalents) or "none", len(survivors), ", ".join(survivors) or "none"))
    print("F3: sweep written to %s: %d mutants, %s, %d equivalent, %d survivors %s" %
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

    # Live: the mutant review cycle 1 found alive, against a green tree
    work = tempfile.mkdtemp(prefix="mps-F3.")
    try:
        tree = os.path.join(work, "tree")
        copy_tree(root, tree)
        code, s, failed, seconds, out = make_check(tree, "clang++", BUILD_LIMIT)
        if code != 0:
            bad.append("make check under clang++ is red without a mutant: %s" % "; ".join(s))
        else:
            mid, _, old, new, _ = [m for m in MUTANTS if m[0] == LIVE][0]
            lucas = os.path.join(tree, "src/lucas.cpp")
            text = open(lucas).read()
            mutated = mutate(text, old, new)
            if mutated is None:
                bad.append("mutant %s: its text does not occur exactly once in src/lucas.cpp" % mid)
            else:
                open(lucas, "w").write(mutated)
                code, s, failed, seconds, out = make_check(tree, "clang++")
                if code == 0:
                    bad.append("live: make check under clang++ stays green with a Jacobi symbol of 0 passing the check (%s)" % "; ".join(s))
                else:
                    live = verdict(code, s, failed)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for b in bad:
        print("F3: " + b)
    if bad:
        sys.exit(1)
    print("F3: the sweep's %d mutants are this tree's, each killed under both compilers or equivalent; live, %s %s under clang++" %
          (len(MUTANTS), LIVE, live))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "sweep":
        sweep(sys.argv[2], sys.argv[3:] or COMPILERS)
    elif len(sys.argv) >= 2 and sys.argv[1] == "check":
        check(sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", ".."))
    else:
        sys.exit("usage: F3-mutants.py sweep SCRATCH | check [ROOT]")
