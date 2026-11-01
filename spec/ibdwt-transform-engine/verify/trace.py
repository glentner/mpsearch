# Gate trace checks: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#   trace.py coverage PHASE TRACE TABLE    the engine tier's calls against what R1, R4, R5 and R14 need
#   trace.py failed LOG LABEL...           each LABEL failed in LOG (a bin/check output)
#   trace.py disagree LOG LABEL...         every tally line of each LABEL in LOG counts every case as
#                                          disagreeing: "   tally <what>: K cases, K disagree"; a
#                                          tally prints that line passing or failing (PLAN § 2.8), so
#                                          a tally that grades nothing the sabotage changes shows as
#                                          "K cases, 0 disagree" instead of passing unseen
#
# TRACE is interpose.cpp's count file ("op p N bit count" lines) from the engine tier run under
# the sabotage mutant; TABLE is `lengths.py table`'s output. Run from the root of the tree under
# test, which holds tests/known/. The trace proves the checks called the engine at every case the
# R-IDs name; the sabotage run, whose every graded check must fail, proves they compared what the
# engine returned.

import sys

bad = []


def fail(msg):
    bad.append(msg)


def read_table(path):
    rows = []
    for line in open(path):
        n, p = line.split()
        rows.append((int(n), int(p)))
    return rows


def assigned(rows):
    out, below = {}, 2
    for n, top in rows:
        first = max(below + 1, n, 3)
        out[n] = (first, top) if first <= top else None
        below = max(below, top)
    return out


def length_for(rows, p):
    for n, top in rows:
        if n <= p <= top and p >= 3:
            return n
    return None


def next_length(rows, n):
    ns = [m for m, _ in rows]
    i = ns.index(n)
    return ns[i + 1] if i + 1 < len(ns) else None


def table_rows(path, ops):
    out = []
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        f = line.split()
        if f[0] in ops and "run=" not in line:
            out.append(f)
    return out


def coverage(phase, trace, table):
    rows = read_table(table)
    calls = {}
    for line in open(trace):
        op, p, n, bit, count = line.split()
        calls[(op, int(p), int(n), bit)] = calls.get((op, int(p), int(n), bit), 0) + int(count)

    def total(op, p, n, bits=None):
        return sum(c for (o, q, m, b), c in calls.items() if o == op and q == p and m == n and (bits is None or b in bits))

    for n, span in assigned(rows).items():
        if span is None:
            fail("length %d is assigned no exponent" % n)
            continue
        for p in span:
            need = [("to_digits", None, 4), ("from_digits", None, 4), ("square", None, 2), ("multiply", None, 2),
                    ("square_subtract", ["0"], 2), ("square_subtract", ["1"], 2), ("square_subtract", [str(p - 1)], 2)]
            for op, bits, k in need:
                if total(op, p, n, bits) < k:
                    fail("exponent %d at length %d: %d call(s) of %s%s, the engine tier needs %d (R1, R4, R14)"
                         % (p, n, total(op, p, n, bits), op, "" if bits is None else " at bit " + bits[0], k))
    if phase < 3:
        return
    p_max = rows[-1][1]
    for f in table_rows("tests/known/partial-residues.txt", ("ll", "prp")):
        op, p, k = f[0], int(f[1]), int(f[3])
        if p > p_max:
            continue
        n = length_for(rows, p)
        m = next_length(rows, n)
        name = "square_subtract" if op == "ll" else "square"
        if total(name, p, n) < 2 * k or m is None or total(name, p, m) < k:
            fail("partial row %s %d %s %d: too few %s calls at lengths %s and %s (R5)" % (op, p, f[2], k, name, n, m))
        if op == "ll" and total(name, p, n, ["other"]) == 0:
            fail("partial row ll %d: no subtraction away from bits 0, 1 and p - 1, so no shifted chain ran (R5)" % p)
    for f in table_rows("tests/known/final-residues.txt", ("ll", "prp1")):
        op, p = f[0], int(f[1])
        if p > 100069:
            continue
        n = length_for(rows, p)
        m = next_length(rows, n)
        name, k = ("square_subtract", p - 2) if op == "ll" else ("square", p)
        if total(name, p, n) < 2 * k or m is None or total(name, p, m) < k:
            fail("final row %s %d: too few %s calls at lengths %s and %s (R5)" % (op, p, name, n, m))
    for f in table_rows("tests/known/mersenne-exponents.txt", ("mersenne",)):
        p = int(f[1])
        if p < 3 or p > 100000:
            continue
        n = length_for(rows, p)
        if total("square_subtract", p, n) < p - 2:
            fail("Mersenne prime exponent %d: fewer than p - 2 Lucas-Lehmer steps at length %s (R5)" % (p, n))


def failed(log, labels):
    text = open(log).read().split("\n")
    fails = {l[len(" FAIL "):] for l in text if l.startswith(" FAIL ")}
    for label in labels:
        if label not in fails:
            fail("'%s' did not fail under the sabotage: it does not grade what the engine returned" % label)


def disagree(log, labels):
    import re
    tallies, label = {}, None
    for line in open(log).read().split("\n"):
        if line.startswith("check "):
            label = line[len("check "):]
            continue
        m = re.match(r"   tally (.+): (\d+) cases, (\d+) disagree$", line)
        if m and label is not None:
            tallies.setdefault(label, []).append((int(m.group(2)), int(m.group(3)), line.strip()))
    for label in labels:
        if label not in tallies:
            fail("'%s' printed no tally line under the sabotage (PLAN § 2.8)" % label)
            continue
        for total, wrong, line in tallies[label]:
            if total != wrong or total == 0:
                fail("'%s': %s — under the sabotage every case must disagree, so the check grades a subset of what it calls" % (label, line))


cmd = sys.argv[1]
if cmd == "coverage":
    coverage(int(sys.argv[2]), sys.argv[3], sys.argv[4])
elif cmd == "failed":
    failed(sys.argv[2], sys.argv[3:])
elif cmd == "disagree":
    disagree(sys.argv[2], sys.argv[3:])
else:
    sys.exit("trace.py: unknown command %s" % cmd)
for b in bad[:30]:
    print("trace.py: " + b, file=sys.stderr)
sys.exit(1 if bad else 0)
