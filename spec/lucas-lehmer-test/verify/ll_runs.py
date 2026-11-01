# bin/mps on the known answers and the refusals, driven by the gate (a cycle record, not a standing
# gate). Run from the root of a built copy of the tree with python3 -I.
#
#   ll_runs.py known BIN [long]   `BIN -t ll p` and `BIN -j -t ll p` for every odd Mersenne prime
#                                 exponent up to 10000 (100000 with `long`) and every `ll` row up to
#                                 20000 (200000): the verdict line's start, the exit status, and the
#                                 record (PLAN § 2.5: its keys in order, the Res64, the shift in
#                                 [0, p), lengths, counts with no failure, record_version 2); R14's
#                                 refusals (2, and with -s 4, 33 and 999999999; 999999937 and P_max + 1
#                                 past the table); six runs of M1279 whose shifts are not all equal (R3)
#   ll_runs.py sabotaged BIN      the same known answers on a bin/mps whose engine's squarings are off
#                                 by one: not one may report its known answer (the verdicts come from
#                                 the engine)
#   ll_runs.py log LOG [long]     tests/ll.sh's output in a make check (make check-long) log: a line for
#                                 every exponent above, each the known answer, and its green summary

import json
import os
import re
import subprocess
import sys

KEYS = ["exponent", "worktype", "status", "res64", "lengths", "shift", "jacobi_checks", "jacobi_failures",
        "degenerate_failures", "confirmation_failures", "transient_faults", "length_retries", "large_distances",
        "largest_distance", "program", "record_version"]


def table(path, op):
    out = []
    for line in open(path):
        f = line.split()
        if f and f[0] == op:
            out.append(f[1:])
    return out


def answers(long):
    up, rows = (100000, 200000) if long else (10000, 20000)
    out = [(int(f[0]), "0000000000000000", True) for f in table("tests/known/mersenne-exponents.txt", "mersenne")
           if 3 <= int(f[0]) <= up]
    out += [(int(f[0]), f[1], False) for f in table("tests/known/final-residues.txt", "ll") if int(f[0]) <= rows]
    return out


def p_max():
    text = open("src/lengths.cpp").read()
    block = text[text.find("largest_exponent_by_length"):]
    block = block[:block.find("};")]
    return max(int(b) for _, b in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", block))


def schedule(p):
    # the Jacobi checks of a clean run: every ceil((p-2)/GOOD_STATES)-th iteration and the last
    m = re.search(r"static const std::uint64_t GOOD_STATES = (\d+);", open("src/lucas.hpp").read())
    last = p - 2
    iv = (last + int(m.group(1)) - 1) // int(m.group(1))
    return last // iv + (0 if last % iv == 0 else 1)


CLEAN = re.compile(r", jacobi (\d+) checked 0 failed, degenerate 0, confirmation 0, transient 0, retries 0, large distances 0, "
                   r"largest distance (0\.\d{4}) \(mps ")


def clean(line, p):
    m = CLEAN.search(line)
    return bool(m) and int(m.group(1)) == schedule(p) and (p < 521 or float(m.group(2)) > 0)


def run(binary, *args):
    r = subprocess.run([binary] + list(args), capture_output=True, text=True, timeout=1800,
                       env=dict({"PATH": "/usr/bin:/bin", "NO_COLOR": "1", "LC_ALL": "C"},
                                **({"GATE_MUTANT": os.environ["GATE_MUTANT"]} if "GATE_MUTANT" in os.environ else {})))
    return r.returncode, r.stdout, r.stderr


def version():
    m = re.search(r'APP_VERSION = "([^"]*)"', open("src/main.hpp").read())
    return m.group(1)


def known(binary, long, bad):
    ver = version()
    for p, res, prime in answers(long):
        code, out, _ = run(binary, "-t", "ll", str(p))
        start = "M%d ll: %s, res64 %s, length" % (p, "prime" if prime else "composite", res)
        if code != (0 if prime else 40) or not out.startswith(start) or out.count("\n") != 1 or not out.rstrip().endswith("(mps %s)" % ver) \
                or not clean(out, p):
            bad.append("mps -t ll %d: exit %d, '%s', want exit %d and a line starting '%s'" % (p, code, out.strip()[:120], 0 if prime else 40, start))
            continue
        code, out, _ = run(binary, "-j", "-t", "ll", str(p))
        try:
            rec = json.loads(out, object_pairs_hook=lambda kv: kv)
            keys = [k for k, _ in rec]
            rec = dict(rec)
        except ValueError:
            bad.append("mps -j -t ll %d: not one JSON record: %s" % (p, out[:120]))
            continue
        if keys != KEYS:
            bad.append("mps -j -t ll %d: keys %s, want %s" % (p, keys, KEYS))
            continue
        ok = (rec["exponent"] == p and rec["worktype"] == "ll" and rec["status"] == ("prime" if prime else "composite")
              and rec["res64"] == res and isinstance(rec["shift"], int) and 0 <= rec["shift"] < p
              and isinstance(rec["lengths"], list) and len(rec["lengths"]) == 1 and isinstance(rec["lengths"][0], int)
              and rec["jacobi_checks"] == schedule(p) and all(rec[k] == 0 for k in KEYS[7:13])
              and isinstance(rec["largest_distance"], float) and 0 <= rec["largest_distance"] <= 0.35
              and (p < 521 or rec["largest_distance"] > 0)
              and dict(rec["program"]) == {"name": "mps", "version": ver} and rec["record_version"] == 2)
        if not ok or code != (0 if prime else 40):
            bad.append("mps -j -t ll %d: exit %d, record %s" % (p, code, out.strip()[:200]))
    pm = p_max()
    refusals = [(["-t", "ll", "2"], "M2 ll: no verdict, ", "odd"),
                (["-s", "-t", "ll", "4"], "M4 ll: no verdict, ", "odd"),
                (["-s", "-t", "ll", "33"], "M33 ll: no verdict, ", "prime exponents"),
                (["-s", "-t", "ll", "999999999"], "M999999999 ll: no verdict, ", str(pm)),
                (["-t", "ll", "999999937"], "M999999937 ll: no verdict, ", str(pm)),
                (["-s", "-t", "ll", str(pm + 1)], "M%d ll: no verdict, " % (pm + 1), str(pm))]
    for args, start, word in refusals:
        code, out, _ = run(binary, *args)
        if code != 42 or not out.startswith(start) or word not in out or out.count("\n") != 1:
            bad.append("mps %s: exit %d, '%s', want exit 42, a no-verdict line naming '%s' (R14)" % (" ".join(args), code, out.strip()[:160], word))
    shifts = set()
    for _ in range(6):
        code, out, _ = run(binary, "-j", "-t", "ll", "1279")
        try:
            shifts.add(json.loads(out)["shift"])
        except (ValueError, KeyError):
            bad.append("mps -j -t ll 1279: no record")
            break
    if len(shifts) < 2:
        bad.append("six runs of mps -t ll 1279 all chose the shift %s: not chosen afresh from a source that differs between runs (R3)" % shifts)


def sabotaged(binary, bad):
    reached = 0
    for p, res, prime in answers(False):
        code, out, _ = run(binary, "-t", "ll", str(p))
        if out.startswith("M%d ll: %s, res64 %s," % (p, "prime" if prime else "composite", res)):
            reached += 1
            bad.append("with every squaring of the engine off by one, mps -t ll %d still reports its known answer" % p)
    return reached


def log(path, long, bad):
    text = open(path, errors="replace").read()
    lines = text.splitlines()
    for p, res, prime in answers(long):
        start = "M%d ll: %s, res64 %s, length" % (p, "prime" if prime else "composite", res)
        got = [l for l in lines if l.startswith(start)]
        if not got:
            bad.append("%s holds no line '%s...'" % (path.split("/")[-1], start))
        elif not clean(got[0], p):
            bad.append("%s: M%d's line is not a clean run of its schedule (every check run, none failed): %s" % (path.split("/")[-1], p, got[0][:160]))
    if not re.search(r"^ll: [1-9][0-9]* passed, 0 failed$", text, re.M):
        bad.append("%s holds no green 'll:' summary" % path.split("/")[-1])


def main(argv):
    bad = []
    mode = argv[1]
    if mode == "known":
        known(argv[2], len(argv) > 3 and argv[3] == "long", bad)
    elif mode == "sabotaged":
        sabotaged(argv[2], bad)
    elif mode == "log":
        log(argv[2], len(argv) > 3 and argv[3] == "long", bad)
    for b in bad[:30]:
        print("runs: " + b, file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
