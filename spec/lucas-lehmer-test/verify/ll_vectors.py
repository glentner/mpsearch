# The Jacobi vectors, recomputed by the gate (a cycle record, not a standing gate). Run from the root
# of a copy of the tree with python3 -I.
#
#   ll_vectors.py check
#
# tests/vectors/jacobi.txt: its envelope (magic, the command line that reproduces it, its sources,
# the trailer's count and SHA-256); every record `jacobi <a> <n> <symbol>` recomputed from the
# operand forms (x:, u:, r:, c:, rebuilt here with the gate's own SplitMix64, Steele, Lea and Flood
# 2014) by the gate's own Jacobi symbol (the reciprocity loop), and, for n = M_p prime, by Euler's
# criterion; and the coverage R5 asks for: for every odd Mersenne prime exponent up to 10000 in
# tests/known/mersenne-exponents.txt and every `ll` row of tests/known/final-residues.txt up to
# 20000, a record modulo M_p (written u:0-p) of each symbol, -1, 1 and 0. The reproduction by the
# file's own command is gate.sh's (a byte comparison).

import hashlib
import re
import sys

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
    if kind == "c":
        limbs = []
        while len(limbs) < n:
            v = next(g)
            for _ in range(1 + (v >> 32) % 32):
                limbs.append(0 if v % 3 == 0 else MASK if v % 3 == 1 else next(g))
        return topped(limbs[:n], bits)
    raise ValueError(spec)


def jacobi(a, n):
    assert n > 0 and n % 2 == 1
    a %= n
    t = 1
    while a:
        while a % 2 == 0:
            a //= 2
            if n % 8 in (3, 5):
                t = -t
        a, n = n, a
        if a % 4 == 3 and n % 4 == 3:
            t = -t
        a %= n
    return t if n == 1 else 0


def table(path, op):
    rows = []
    for line in open(path):
        f = line.split()
        if f and f[0] == op:
            rows.append(int(f[1]))
    return rows


def main(argv):
    bad = []
    primes = sorted(set(table("tests/known/mersenne-exponents.txt", "mersenne")))
    want = sorted(set([p for p in primes if 3 <= p <= 10000] + [p for p in table("tests/known/final-residues.txt", "ll") if p <= 20000]))
    raw = open("tests/vectors/jacobi.txt", "rb").read()
    lines = raw.decode("ascii").split("\n")
    if lines[-1] != "":
        bad.append("jacobi.txt does not end in a newline")
    lines = lines[:-1]
    if lines[0] != "# mps vectors 1":
        bad.append("jacobi.txt: no magic line")
    if lines[1] != "# command: python3 tests/vectors/generate.py jacobi > tests/vectors/jacobi.txt":
        bad.append("jacobi.txt: its second line is not the command that reproduces it (R5)")
    body = "".join(l + "\n" for l in lines[:-1]).encode("ascii")
    records = [l for l in lines if not l.startswith("#")]
    trailer = "# end %d records %s" % (len(records), hashlib.sha256(body).hexdigest())
    if lines[-1] != trailer:
        bad.append("jacobi.txt: the trailer is not '%s'" % trailer)
    seen = {}
    wrong = 0
    prime_set = set(primes)
    for rec in records:
        f = rec.split()
        if len(f) != 4 or f[0] != "jacobi" or f[3] not in ("-1", "0", "1"):
            bad.append("jacobi.txt: not a jacobi record: %s" % rec[:80])
            continue
        a, n = operand(f[1]), operand(f[2])
        if n % 2 == 0 or n <= 0:
            bad.append("jacobi.txt: an even or zero modulus: %s" % rec[:80])
            continue
        j = jacobi(a, n)
        m = re.fullmatch(r"u:0-(\d+)", f[2])
        if m and int(m.group(1)) in prime_set and int(m.group(1)) > 2:
            e = pow(a % n, (n - 1) // 2, n)
            euler = 0 if a % n == 0 else 1 if e == 1 else -1 if e == n - 1 else None
            if euler != j:
                bad.append("jacobi.txt: the reciprocity loop and Euler's criterion disagree on %s" % rec[:80])
        if j != int(f[3]):
            wrong += 1
            if wrong <= 5:
                bad.append("jacobi.txt: %s holds %s, the gate computes %d" % (rec[:60], f[3], j))
        if m:
            seen.setdefault(int(m.group(1)), set()).add(int(f[3]))
    if wrong > 5:
        bad.append("jacobi.txt: %d records disagree with the gate in all" % wrong)
    for p in want:
        got = seen.get(p, set())
        for s in (-1, 0, 1):
            if s not in got:
                bad.append("jacobi.txt: no record modulo M_%d with symbol %d (R5)" % (p, s))
    for b in bad:
        print("vectors: " + b, file=sys.stderr)
    if not bad:
        print("vectors: %d records recomputed; %d exponents covered with every symbol" % (len(records), len(want)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
