# Review cycle 1, F1 (a cycle record, not a standing gate): every refusal of `mps -t ll` names every
# reason that applies to its exponent, in every combination, and no other; and the note says so.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F1-refusals.py [ROOT]
#
# Builds bin/mps with clang++ from a copy of ROOT's tree as a clone would hold it (untracked files
# included) in a temporary directory, then runs `bin/mps -s -t ll p` over a domain that holds every
# combination of the three reasons (not odd or below 3; past P_max; odd and composite): every p from
# 2 to 64 but the odd primes, P_max - 3 to P_max + 64 but the odd primes at or below P_max, the top
# of the command line's range, and 200 more past P_max drawn from a fixed seed. Each must exit 42
# with one no-verdict line naming exactly the reasons that apply, P_max by its value, as this
# script computes them on its own (P_max read from src/lengths.cpp, primality by trial division).
# doc/lucas-lehmer.md must say that a refusal names every reason that applies and must not carry
# the sentence F1 found false. Prints what is wrong and exits 1, or prints one line and exits 0.

import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
bad = []


def p_max(tree):
    text = open(os.path.join(tree, "src/lengths.cpp")).read()
    block = text[text.find("largest_exponent_by_length"):]
    block = block[:block.find("};")]
    return max(int(b) for _, b in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", block))


def prime(n):
    if n < 2:
        return False
    if n % 2 == 0:
        return n == 2
    d = 3
    while d * d <= n:
        if n % d == 0:
            return False
        d += 2
    return True


note = open(os.path.join(root, "doc/lucas-lehmer.md")).read()
if "both past P_max and composite gets both reasons" in note:
    bad.append("doc/lucas-lehmer.md still says only a p both past P_max and composite gets two reasons")
if "names every reason that applies" not in note:
    bad.append("doc/lucas-lehmer.md does not say that a refusal names every reason that applies")

work = tempfile.mkdtemp(prefix="mps-F1.")
try:
    tree = os.path.join(work, "tree")
    os.mkdir(tree)
    listed = subprocess.run(["git", "-C", root, "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
                            capture_output=True, check=True).stdout.split(b"\0")
    for rel in (r.decode() for r in listed if r):
        src = os.path.join(root, rel)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(os.path.join(tree, rel)), exist_ok=True)
            shutil.copy2(src, os.path.join(tree, rel))
    built = subprocess.run(["/usr/bin/make", "-C", tree, "CXX=clang++", "bin/mps"], capture_output=True, text=True, timeout=900)
    if built.returncode != 0:
        print(built.stdout[-2000:] + built.stderr[-2000:])
        sys.exit("F1: bin/mps does not build")
    mps = os.path.join(tree, "bin/mps")
    pm = p_max(tree)

    domain = [p for p in range(2, 65)] + list(range(pm - 3, pm + 65)) + [999999929, 999999937, 999999998, 999999999]
    draw = random.Random(20261101)
    domain += [draw.randrange(pm + 1, 1000000000) for _ in range(200)]
    combos = {}
    runs = 0
    for p in sorted(set(domain)):
        applies = {"odd": p < 3 or p % 2 == 0, "past": p > pm, "composite": p >= 3 and p % 2 == 1 and not prime(p)}
        if not any(applies.values()):
            continue
        words = {"odd": "the Lucas-Lehmer test applies to odd exponents of 3 or more",
                 "past": "the exponent is past %d, the largest the length table holds (P_max)" % pm,
                 "composite": "the Lucas-Lehmer test runs on prime exponents, and %d is composite" % p}
        r = subprocess.run([mps, "-s", "-t", "ll", str(p)], capture_output=True, text=True, timeout=60)
        runs += 1
        key = " and ".join(k for k in ("odd", "past", "composite") if applies[k])
        combos[key] = combos.get(key, 0) + 1
        out = r.stdout
        start = "M%d ll: no verdict, " % p
        if r.returncode != 42 or not out.startswith(start) or out.count("\n") != 1:
            bad.append("mps -s -t ll %d: exit %d, '%s', want exit 42 and one no-verdict line" % (p, r.returncode, out.strip()[:200]))
            continue
        for k, w in words.items():
            if (w in out) != applies[k]:
                bad.append("mps -s -t ll %d (%s): the line %s the %s reason: '%s'" % (p, key, "lacks" if applies[k] else "names", k, out.strip()))
    for want in ("odd", "past", "composite", "odd and past", "past and composite"):
        if want not in combos:
            bad.append("the domain held no exponent with the reasons: %s" % want)
finally:
    shutil.rmtree(work, ignore_errors=True)

for b in bad[:40]:
    print("F1: " + b)
if bad:
    print("F1: %d failures" % len(bad))
    sys.exit(1)
print("F1: %d refusals name every reason that applies (%s), and the note says so" %
      (runs, ", ".join("%s %d" % kv for kv in sorted(combos.items()))))
