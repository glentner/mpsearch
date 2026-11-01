# Review cycle 2, F6 (a cycle record, not a standing gate): the Jacobi vectors reach every carry and
# borrow path of the symbol's steps on limbs, and no decision or arithmetic statement on the Jacobi
# code path of src/mp.cpp can change while make check stays green.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py sweep SCRATCH [COMPILER...]
#       Every mutant below, each under make check with clang++ and with g++-16, in two copies of
#       the tree under SCRATCH (a directory outside the repository); writes the record,
#       F6-mutants.txt beside this script. About 40 minutes on an M2 Mac. Naming compilers runs
#       those alone and writes a partial record under SCRATCH instead.
#   python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py check [ROOT]
#       The checker: (1) the paths: every record of tests/vectors/jacobi.txt whose modulus has at
#       most 2048 bits, traced by this script's own model of jacobi on limbs (compare_limbs,
#       strip_twos, subtract_in_place and the gcd, as src/mp.cpp runs them), gives the record's
#       symbol, and the records together take every path in PATHS; (2) the record is of this tree
#       (the digests of the files below and of the mutant table are the record's), and every mutant
#       in it was killed under both compilers or is listed as equivalent with its reason; (3) live,
#       in a copy of the tree in a temporary directory, make check under clang++ is green as it is
#       and red with the borrow through an equal limb dropped (the mutant review cycle 2 found
#       alive). About 1 minute.
#
# The sweep covers compare_limbs, trim, strip_twos, subtract_in_place and jacobi. The reduction of
# a >= n goes through reduce, the long division, whose own vectors and checks are the modular
# family's; it is not swept here. A mutant is one exact edit of src/mp.cpp: its text must occur
# exactly once. It is killed when make check exits other than 0 or runs past its time limit. Before
# each make the objects and binaries built from src/mp.cpp are deleted, so a mutant is always
# compiled, whatever the timestamps; a make check past its limit is killed with its process group.
# Prints what is wrong and exits 1, or prints one line and exits 0.

import hashlib
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
RECORD = os.path.join(HERE, "F6-mutants.txt")
MAKE = "/usr/bin/make"
COMPILERS = ["clang++", "g++-16"]
LIMIT = 90           # a mutant's make check, which takes 10 to 25 s on an M2 Mac with mp.cpp rebuilt; many loop forever
BUILD_LIMIT = 900    # a make check that builds everything
DIGESTED = ["src/mp.cpp", "src/mp.hpp", "tests/vectors/jacobi.txt", "tests/vectors/generate.py", "tests/check_mp.cpp"]
REBUILT = ["lib/mp.o", "lib/check/mp.o", "bin/mps", "bin/check"]
LIVE = "subtract-borrow-below-only"
TRACED_BITS = 2048

# The paths of jacobi's steps on limbs that the vectors must take
PATHS = [
    "compare: unequal limb counts",
    "compare: decided below the top limb",
    "compare: equal",
    "strip: one whole limb",
    "strip: two or more whole limbs",
    "strip: bits carried down from the next limb",
    "strip: the top limb emptied and trimmed",
    "subtract: equal limbs below the top, nothing borrowed",
    "subtract: a borrow across an equal limb",
    "subtract: a borrow across two or more limbs in a row",
    "subtract: a borrow that ends at a limb one above",
    "subtract: a borrow into the top limb",
    "subtract: a borrow past the shorter operand",
    "subtract: a borrow across a zero limb past the shorter operand",
    "subtract: the longer operand, nothing borrowed past the shorter",
    "subtract: trimmed by two or more limbs",
    "symbol: a gcd of several limbs whose low limb is 1",
]

SUB = "\t\tx[i] = difference - borrow;\n\t\tborrow = below + (difference < borrow);\n"

# (id, statement, old text, new text, why it is equivalent or None)
MUTANTS = [
    # compare_limbs
    ("compare-count-sign", "compare_limbs", "return x.size() < y.size() ? -1 : 1;", "return x.size() < y.size() ? 1 : -1;", None),
    ("compare-count-ignored", "compare_limbs", "\tif (x.size() != y.size())\n\t\treturn x.size()", "\tif (false)\n\t\treturn x.size()", None),
    ("compare-top-limb-only", "compare_limbs", "for (std::size_t i = x.size(); i-- > 0;)",
     "for (std::size_t i = x.size(); i-- > 0 and i + 1 == x.size();)", None),
    ("compare-bottom-up", "compare_limbs", "for (std::size_t i = x.size(); i-- > 0;)", "for (std::size_t i = 0; i < x.size(); i++)", None),
    ("compare-limb-sign", "compare_limbs", "return x[i] < y[i] ? -1 : 1;", "return x[i] < y[i] ? 1 : -1;", None),
    ("compare-equal-as-above", "compare_limbs", "\t\t\treturn x[i] < y[i] ? -1 : 1;\n\treturn 0;", "\t\t\treturn x[i] < y[i] ? -1 : 1;\n\treturn 1;", None),
    # trim
    ("trim-one-limb", "trim", "\twhile (not x.empty() and x.back() == 0)", "\tif (not x.empty() and x.back() == 0)", None),
    ("trim-off", "trim", "\twhile (not x.empty() and x.back() == 0)\n\t\tx.pop_back();\n", "", None),
    # strip_twos
    ("strip-whole-off", "strip_twos", "\twhile (x[whole] == 0)\n\t\twhole++;\n", "", None),
    ("strip-whole-once", "strip_twos", "\twhile (x[whole] == 0)", "\tif (x[whole] == 0)", None),
    ("strip-part-off", "strip_twos", "std::size_t part = std::countr_zero(x[whole]);", "std::size_t part = 0;", None),
    ("strip-shift-one-more", "strip_twos", "std::uint64_t limb = x[i] >> part;", "std::uint64_t limb = x[i] >> part >> 1;", None),
    ("strip-carry-off", "strip_twos", "\t\tif (part != 0 and i + 1 < x.size())\n\t\t\tlimb |= x[i + 1] << (64 - part);\n", "", None),
    ("strip-carry-not-from-top", "strip_twos", "if (part != 0 and i + 1 < x.size())", "if (part != 0 and i + 2 < x.size())", None),
    ("strip-carry-at-part-0", "strip_twos", "if (part != 0 and i + 1 < x.size())", "if (i + 1 < x.size())", None),
    ("strip-carry-shift", "strip_twos", "limb |= x[i + 1] << (64 - part);", "limb |= x[i + 1] << (63 - part);", None),
    ("strip-resize-off", "strip_twos", "\tx.resize(x.size() - whole);\n", "", None),
    ("strip-trim-off", "strip_twos", "\ttrim(x);\n\treturn 64 * whole + part;", "\treturn 64 * whole + part;", None),
    ("strip-count-limbs", "strip_twos", "\treturn 64 * whole + part;", "\treturn whole + part;", None),
    ("strip-count-bits-only", "strip_twos", "\treturn 64 * whole + part;", "\treturn part;",
     "jacobi reads only the count's parity, and 64 * whole is even, so 64 * whole + part and part have the same parity"),
    ("strip-count-extra-limb", "strip_twos", "\treturn 64 * whole + part;", "\treturn 64 * whole + part + 64 * (whole > 0);",
     "jacobi reads only the count's parity, and the extra 64 is even"),
    # subtract_in_place
    ("subtract-borrow-below-only", "subtract_in_place", SUB, "\t\tx[i] = difference - borrow;\n\t\tborrow = below;\n", None),
    ("subtract-borrow-at-equal", "subtract_in_place", SUB, "\t\tx[i] = difference - borrow;\n\t\tborrow = below + (difference <= borrow);\n", None),
    ("subtract-borrow-carried-only", "subtract_in_place", SUB, "\t\tx[i] = difference - borrow;\n\t\tborrow = (difference < borrow);\n", None),
    ("subtract-borrow-or", "subtract_in_place", SUB, "\t\tx[i] = difference - borrow;\n\t\tborrow = below | (difference < borrow);\n",
     "the two terms are never both 1: below = 1 means x[i] < y[i], so difference = x[i] - y[i] + 2^64 is not 0, and "
     "difference < borrow needs difference = 0 (borrow is 0 or 1); so the sum and the or are equal"),
    ("subtract-borrow-not-subtracted", "subtract_in_place", SUB, "\t\tx[i] = difference;\n\t\tborrow = below + (difference < borrow);\n", None),
    ("subtract-stops-at-y", "subtract_in_place", "(i < y.size() or borrow != 0)", "(i < y.size())", None),
    ("subtract-runs-every-limb", "subtract_in_place", "(i < y.size() or borrow != 0)", "(true)",
     "past y's limbs with nothing borrowed, each limb of x has 0 subtracted with no borrow, so it is written back "
     "unchanged and the borrow stays 0: the extra passes change nothing"),
    ("subtract-y-past-its-end", "subtract_in_place", "\t\tstd::uint64_t yi = i < y.size() ? y[i] : 0;\n\t\tstd::uint64_t difference = x[i] - yi;\n\t\tstd::uint64_t below = x[i] < yi;\n\t\tx[i] = difference - borrow;\n\t\tborrow = below + (difference < borrow);\n\t}\n\ttrim(x);",
     "\t\tstd::uint64_t yi = y[i];\n\t\tstd::uint64_t difference = x[i] - yi;\n\t\tstd::uint64_t below = x[i] < yi;\n\t\tx[i] = difference - borrow;\n\t\tborrow = below + (difference < borrow);\n\t}\n\ttrim(x);", None),
    ("subtract-below-at-equal", "subtract_in_place", "\t\tstd::uint64_t below = x[i] < yi;\n\t\tx[i] = difference - borrow;", "\t\tstd::uint64_t below = x[i] <= yi;\n\t\tx[i] = difference - borrow;", None),
    ("subtract-trim-off", "subtract_in_place", "\t\tborrow = below + (difference < borrow);\n\t}\n\ttrim(x);", "\t\tborrow = below + (difference < borrow);\n\t}", None),
    ("subtract-borrow-starts-1", "subtract_in_place", "\tstd::uint64_t borrow = 0;\n\tfor (std::size_t i = 0; i < x.size() and", "\tstd::uint64_t borrow = 1;\n\tfor (std::size_t i = 0; i < x.size() and", None),
    # jacobi
    ("jacobi-zero-modulus-unrefused", "jacobi", "if (n.is_zero())", "if (false)", None),
    ("jacobi-even-modulus-unrefused", "jacobi", "if (n.limbs()[0] % 2 == 0)", "if (false)", None),
    ("jacobi-always-reduced", "jacobi", "compare(a, n) < 0 ? a.limbs() : reduce(a, n).limbs()", "reduce(a, n).limbs()",
     "reduce returns a itself when a < n, so reducing every a gives the same limbs"),
    ("jacobi-never-reduced", "jacobi", "compare(a, n) < 0 ? a.limbs() : reduce(a, n).limbs()", "a.limbs()",
     "every step of the loop keeps the symbol whatever x's size (stripping 2s by the symbol of 2, swapping by reciprocity, "
     "subtracting y), and while x >= y it subtracts y, so from x = a it reaches the same symbol as from a mod n, only later"),
    ("jacobi-reduced-at-equal", "jacobi", "compare(a, n) < 0 ? a.limbs() : reduce(a, n).limbs()", "compare(a, n) <= 0 ? a.limbs() : reduce(a, n).limbs()",
     "only a = n changes: x starts at n rather than empty, the first pass subtracts x = y and ends with y = n, so the "
     "symbol is 0 unless n = 1, where both give 1"),
    ("jacobi-y-mod-4", "jacobi", "std::uint64_t y8 = y[0] % 8;", "std::uint64_t y8 = y[0] % 4;", None),
    ("jacobi-two-sign-3-or-7", "jacobi", "(y8 == 3 or y8 == 5)", "(y8 == 3 or y8 == 7)", None),
    ("jacobi-two-sign-even-count", "jacobi", "if (strip_twos(x) % 2 == 1 and", "if (strip_twos(x) % 2 == 0 and", None),
    ("jacobi-two-sign-off", "jacobi", "(y8 == 3 or y8 == 5))\n\t\t\tt = -t;", "(y8 == 3 or y8 == 5))\n\t\t\t;", None),
    ("jacobi-swap-off", "jacobi", "if (compare_limbs(x, y) < 0) {", "if (false) {", None),
    ("jacobi-swap-at-equal", "jacobi", "if (compare_limbs(x, y) < 0) {", "if (compare_limbs(x, y) <= 0) {",
     "x = y swaps two equal vectors, which changes nothing but the sign: and x = y > 1 makes the gcd y > 1, so the "
     "symbol is 0 whatever the sign, while x = y = 1 is 1 mod 4 and flips nothing"),
    ("jacobi-reciprocity-either", "jacobi", "if (x[0] % 4 == 3 and y[0] % 4 == 3)", "if (x[0] % 4 == 3 or y[0] % 4 == 3)", None),
    ("jacobi-reciprocity-off", "jacobi", "if (x[0] % 4 == 3 and y[0] % 4 == 3)", "if (false)", None),
    ("jacobi-gcd-unread", "jacobi", "return y.size() == 1 and y[0] == 1 ? t : 0;", "return t;", None),
    ("jacobi-gcd-low-limb-only", "jacobi", "return y.size() == 1 and y[0] == 1 ? t : 0;", "return y[0] == 1 ? t : 0;", None),
    ("jacobi-gcd-one-limb-only", "jacobi", "return y.size() == 1 and y[0] == 1 ? t : 0;", "return y.size() == 1 ? t : 0;", None),
    ("jacobi-sign-starts-minus", "jacobi", "int t = 1;", "int t = -1;", None),
    ("jacobi-loop-one-limb-early", "jacobi", "while (not x.empty()) {", "while (x.size() > 1) {", None),
]


# --- The paths -----------------------------------------------------------------------------------

MASK = (1 << 64) - 1


def splitmix(seed):
    state = seed & MASK
    while True:
        state = (state + 0x9E3779B97F4A7C15) & MASK
        z = state
        z = ((z ^ (z >> 33)) * 0xFF51AFD7ED558CCD) & MASK
        z = ((z ^ (z >> 33)) * 0xC4CEB9FE1A85EC53) & MASK
        yield z ^ (z >> 33)


def topped(limbs, bits):
    v = 0
    for i, l in enumerate(limbs[:(bits + 63) // 64]):
        v |= l << (64 * i)
    return (v & ((1 << bits) - 1)) | (1 << (bits - 1))


def operand(spec):
    # tests/vectors/generate.py's operand forms: x: in full, u: bit ranges, r: and c: SplitMix64 seeds
    kind, _, rest = spec.partition(":")
    if kind == "x":
        return int(rest, 16)
    if kind == "u":
        v = 0
        for part in rest.split(","):
            lo, hi = (int(t) for t in part.split("-"))
            v |= ((1 << (hi - lo)) - 1) << lo
        return v
    bits, seed = (int(t) for t in rest.split(":"))
    g = splitmix(seed)
    n = (bits + 63) // 64
    if kind == "r":
        return topped([next(g) for _ in range(n)], bits)
    limbs = []
    while len(limbs) < n:
        v = next(g)
        for _ in range(1 + (v >> 32) % 32):
            limbs.append(0 if v % 3 == 0 else MASK if v % 3 == 1 else next(g))
    return topped(limbs[:n], bits)


def traced(a, n):
    # jacobi(a, n) on limbs as src/mp.cpp runs it: the symbol and the set of PATHS it takes
    seen = set()

    def limbs_of(v):
        out = []
        while v:
            out.append(v & MASK)
            v >>= 64
        return out

    def trim(v):
        k = 0
        while v and v[-1] == 0:
            v.pop()
            k += 1
        return k

    def compare_limbs(v, w):
        if len(v) != len(w):
            seen.add("compare: unequal limb counts")
            return -1 if len(v) < len(w) else 1
        for i in reversed(range(len(v))):
            if v[i] != w[i]:
                if i < len(v) - 1:
                    seen.add("compare: decided below the top limb")
                return -1 if v[i] < w[i] else 1
        seen.add("compare: equal")
        return 0

    def strip_twos(v):
        whole = 0
        while v[whole] == 0:
            whole += 1
        if whole == 1:
            seen.add("strip: one whole limb")
        elif whole > 1:
            seen.add("strip: two or more whole limbs")
        part = 0
        while not (v[whole] >> part) & 1:
            part += 1
        for i in range(whole, len(v)):
            limb = v[i] >> part
            if part and i + 1 < len(v):
                limb |= (v[i + 1] << (64 - part)) & MASK
                seen.add("strip: bits carried down from the next limb")
            v[i - whole] = limb
        del v[len(v) - whole:]
        if trim(v):
            seen.add("strip: the top limb emptied and trimmed")
        return 64 * whole + part

    def subtract_in_place(v, w):
        same = v == w
        borrow, row, i = 0, 0, 0
        while i < len(v) and (i < len(w) or borrow):
            wi = w[i] if i < len(w) else 0
            if borrow and i >= len(w):
                seen.add("subtract: a borrow past the shorter operand")
            if borrow and i == len(v) - 1:
                seen.add("subtract: a borrow into the top limb")
            if borrow and (v[i] - wi) & MASK == 1:
                seen.add("subtract: a borrow that ends at a limb one above")
            if borrow and v[i] == wi:
                seen.add("subtract: a borrow across an equal limb" if i < len(w)
                         else "subtract: a borrow across a zero limb past the shorter operand")
                row += 1
                if row > 1:
                    seen.add("subtract: a borrow across two or more limbs in a row")
            else:
                row = 0
            if not borrow and v[i] == wi and i < len(w) and i < len(v) - 1 and not same:
                seen.add("subtract: equal limbs below the top, nothing borrowed")
            d = (v[i] - wi) & MASK
            below = int(v[i] < wi)
            v[i] = (d - borrow) & MASK
            borrow = below + int(d < borrow)
            i += 1
        if i < len(v):
            seen.add("subtract: the longer operand, nothing borrowed past the shorter")
        if trim(v) > 1:
            seen.add("subtract: trimmed by two or more limbs")

    x = limbs_of(a if a < n else a % n)
    y = limbs_of(n)
    t = 1
    while x:
        y8 = y[0] % 8
        if strip_twos(x) % 2 == 1 and y8 in (3, 5):
            t = -t
        if compare_limbs(x, y) < 0:
            x, y = y, x
            if x[0] % 4 == 3 and y[0] % 4 == 3:
                t = -t
        subtract_in_place(x, y)
    if len(y) > 1 and y[0] == 1:
        seen.add("symbol: a gcd of several limbs whose low limb is 1")
    return (t if y == [1] else 0), seen


def paths_check(root, bad):
    reached = {}
    traced_records = 0
    for line in open(os.path.join(root, "tests/vectors/jacobi.txt")):
        f = line.split()
        if not f or f[0] != "jacobi":
            continue
        a, n = operand(f[1]), operand(f[2])
        if n.bit_length() > TRACED_BITS:
            continue
        traced_records += 1
        symbol, seen = traced(a, n)
        if symbol != int(f[3]):
            bad.append("paths: the model of jacobi on limbs gives %d for %s, the record %s" % (symbol, line.strip()[:80], f[3]))
        for s in seen:
            reached[s] = reached.get(s, 0) + 1
    missing = [p for p in PATHS if p not in reached]
    for p in missing:
        bad.append("paths: no record of tests/vectors/jacobi.txt takes the path '%s'" % p)
    return traced_records, len(PATHS) - len(missing)


# --- The mutants ---------------------------------------------------------------------------------

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
    record = RECORD if compilers == COMPILERS else os.path.join(os.path.abspath(scratch), "F6-mutants.partial.txt")
    root = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
    scratch = os.path.abspath(scratch)
    if scratch.startswith(root + os.sep) or scratch == root:
        sys.exit("F6: the scratch directory must be outside the repository")
    original = open(os.path.join(root, "src/mp.cpp")).read()
    for mid, _, old, new, _ in MUTANTS:
        if mutate(original, old, new) is None:
            sys.exit("F6: mutant %s: its text occurs %d times in src/mp.cpp, want once" % (mid, original.count(old)))
    trees = {}
    for cxx in compilers:
        tree = os.path.join(scratch, "tree-" + cxx)
        if os.path.exists(tree):
            shutil.rmtree(tree)
        copy_tree(root, tree)
        code, sums, failed, seconds, out = make_check(tree, cxx, BUILD_LIMIT)
        if code != 0:
            print(out[-3000:])
            sys.exit("F6: make check under %s is red without a mutant" % cxx)
        print("baseline %s: green in %.0f s (%s)" % (cxx, seconds, "; ".join(sums)), flush=True)
        trees[cxx] = tree
    rows = []
    for mid, statement, old, new, equivalent in MUTANTS:
        results = []
        for cxx in compilers:
            mp = os.path.join(trees[cxx], "src/mp.cpp")
            open(mp, "w").write(mutate(original, old, new))
            code, sums, failed, seconds, _ = make_check(trees[cxx], cxx)
            open(mp, "w").write(original)
            results.append("%s: %s, %.0f s" % (cxx, verdict(code, sums, failed), seconds))
        row = "%s\t%s\t%s" % (mid, statement, "\t".join(results))
        if equivalent:
            row += "\tequivalent: " + equivalent
        rows.append(row)
        print(row, flush=True)
    for cxx in compilers:
        code, _, _, _, _ = make_check(trees[cxx], cxx, BUILD_LIMIT)
        if code != 0:
            sys.exit("F6: make check under %s is red after the sweep restored src/mp.cpp" % cxx)
    killed = {cxx: sum(1 for r in rows if ("%s: killed" % cxx) in r) for cxx in compilers}
    survivors = [r.split("\t")[0] for r in rows if "survived" in r and "\tequivalent: " not in r]
    equivalents = [r.split("\t")[0] for r in rows if "\tequivalent: " in r]
    with open(record, "w") as f:
        f.write("# F6's mutant sweep over the Jacobi code path of src/mp.cpp (review cycle 2), written by\n")
        f.write("#   python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py sweep SCRATCH\n")
        f.write("# Each mutant under make check with clang++ and g++-16; killed = make check exited other than 0.\n")
        f.write("# Columns: id, statement, clang++ result, g++-16 result, and for an equivalent mutant why.\n")
        for rel in DIGESTED:
            f.write("sha256 %s %s\n" % (rel, digest(os.path.join(root, rel))))
        f.write("sha256 mutant-table %s\n" % table_digest())
        for r in rows:
            f.write(r + "\n")
        f.write("# %d mutants: %s; %d equivalent (%s); %d not equivalent survived (%s)\n" %
                (len(rows), ", ".join("%d killed under %s" % (killed[c], c) for c in compilers), len(equivalents),
                 ", ".join(equivalents) or "none", len(survivors), ", ".join(survivors) or "none"))
    print("F6: sweep written to %s: %d mutants, %s, %d equivalent, %d survivors %s" %
          (record, len(rows), ", ".join("%d killed under %s" % (killed[c], c) for c in compilers),
           len(equivalents), len(survivors), survivors))
    sys.exit(1 if survivors else 0)


def check(root):
    root = os.path.abspath(root)
    bad = []
    traced_records, reached = paths_check(root, bad)
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

    # Live: the mutant review cycle 2 found alive, against a green tree
    live = None
    work = tempfile.mkdtemp(prefix="mps-F6.")
    try:
        tree = os.path.join(work, "tree")
        copy_tree(root, tree)
        code, s, failed, seconds, out = make_check(tree, "clang++", BUILD_LIMIT)
        if code != 0:
            bad.append("make check under clang++ is red without a mutant: %s" % "; ".join(s))
        else:
            mid, _, old, new, _ = [m for m in MUTANTS if m[0] == LIVE][0]
            mp = os.path.join(tree, "src/mp.cpp")
            mutated = mutate(open(mp).read(), old, new)
            if mutated is None:
                bad.append("mutant %s: its text does not occur exactly once in src/mp.cpp" % mid)
            else:
                open(mp, "w").write(mutated)
                code, s, failed, seconds, out = make_check(tree, "clang++")
                if code == 0:
                    bad.append("live: make check under clang++ stays green with the borrow through an equal limb dropped (%s)" % "; ".join(s))
                else:
                    live = verdict(code, s, failed)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for b in bad:
        print("F6: " + b)
    if bad:
        sys.exit(1)
    print("F6: %d records traced take all %d paths; the sweep's %d mutants are this tree's, each killed under both compilers "
          "or equivalent; live, %s %s under clang++" % (traced_records, reached, len(MUTANTS), LIVE, live))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "sweep":
        sweep(sys.argv[2], sys.argv[3:] or COMPILERS)
    elif len(sys.argv) >= 2 and sys.argv[1] == "check":
        check(sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "..", ".."))
    else:
        sys.exit("usage: F6-mutants.py sweep SCRATCH | check [ROOT]")
