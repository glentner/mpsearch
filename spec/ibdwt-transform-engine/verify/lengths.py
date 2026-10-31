# Gate length-table checks: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#   lengths.py check                 src/lengths.txt and src/lengths.cpp against each other (R3)
#   lengths.py sample quick|full     the boundary probes to measure again, "<length> <exponent>" each
#   lengths.py maxima quick|full     the table's maxima to measure independently, "<length> <exponent> <squarings>"
#   lengths.py compare FILE          FILE's probe lines against the same probes in src/lengths.txt
#   lengths.py table                 the table's rows, "<length> <largest exponent>" each
#
# Run from the root of the tree under test. The rule is PLAN § 2.4's, written here apart from the
# builder's: each supported length's probes are the consecutive grid exponents ceil(N i / 16) up to
# the first probe that fails (distance above 0.35, or magnitude 2^50 or more), every held probe took
# max(64, ceil(2^22 / N)) squarings of N outputs each and the failing one no more, and the table
# gives the length the exponent of the probe 2 places below the last held one, and that probe's rms
# as its envelope.

import math
import re
import sys

DATA = "src/lengths.txt"
TABLE = "src/lengths.cpp"
SIZING_DISTANCE = 0.35
SIZING_MAGNITUDE = 2.0 ** 50
STEPS = 16
P_FLOOR = 10000019
PROBE_SQUARINGS = 64
PROBE_OUTPUTS = 1 << 22
BACKOFF = 2
QUICK = [1, 3, 15, 16, 640, 13 * 1024, 65536, 1048576]

bad = []


def fail(msg):
    bad.append(msg)


def lengths():
    return sorted(m << k for m in range(1, 16, 2) for k in range(21) if (m << k) <= 1 << 20)


def held(d, mag):
    return d <= SIZING_DISTANCE and mag < SIZING_MAGNITUDE


def read_data():
    lines = open(DATA).read().split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if not lines or lines[0] != "# mps lengths 2":
        fail("%s: the first line is not '# mps lengths 2'" % DATA)
    if len(lines) < 2 or not lines[1].startswith("# command: make lengths"):
        fail("%s: the second line does not give the command, '# command: make lengths ...' (R3)" % DATA)
    rows, text, trailer = [], {}, None
    for n, line in enumerate(lines, 1):
        if line.startswith("#"):
            m = re.fullmatch(r"# end (\d+) probes", line)
            if m:
                trailer = int(m.group(1))
                if n != len(lines):
                    fail("%s:%d: the trailer is not the last line" % (DATA, n))
            continue
        f = line.split(" ")
        if len(f) != 7 or not all(x.isdigit() for x in f[:4]):
            fail("%s:%d: not '<length> <exponent> <squarings> <outputs> <distance> <magnitude> <rms>': %r" % (DATA, n, line))
            continue
        try:
            d, mag, rms = float(f[4]), float(f[5]), float(f[6])
        except ValueError:
            fail("%s:%d: the distance, magnitude or rms is not a number" % (DATA, n))
            continue
        if not (0 <= d <= 1) or not (0 < mag) or not (0 < rms <= mag):
            fail("%s:%d: a distance outside [0, 1], or a magnitude or rms not above 0, or an rms above the magnitude" % (DATA, n))
        rows.append((int(f[0]), int(f[1]), d, mag, int(f[2]), int(f[3]), rms))
        text[(int(f[0]), int(f[1]))] = line
    if trailer is None or trailer != len(rows):
        fail("%s: the trailer does not count the %d probes" % (DATA, len(rows)))
    return rows, text


def read_table():
    src = open(TABLE).read()
    m = re.search(r"static std::map<std::uint64_t, std::uint64_t>\s*\nlargest_exponent_by_length \{\n(.*?)\n\};", src, re.S)
    if not m:
        fail("%s: no `static std::map<std::uint64_t, std::uint64_t>` named largest_exponent_by_length" % TABLE)
        return {}
    table = {}
    for line in m.group(1).split("\n"):
        r = re.fullmatch(r"\t\{\s*(\d+), (\d+)\},", line)
        if not r:
            fail("%s: a table line that is not '\\t{<length>, <exponent>},': %r" % (TABLE, line))
            continue
        table[int(r.group(1))] = int(r.group(2))
    return table


def read_envelope():
    src = open(TABLE).read()
    m = re.search(r"static std::map<std::uint64_t, double>\s*\nrms_envelope_by_length \{\n(.*?)\n\};", src, re.S)
    if not m:
        fail("%s: no `static std::map<std::uint64_t, double>` named rms_envelope_by_length" % TABLE)
        return {}
    env = {}
    for line in m.group(1).split("\n"):
        r = re.fullmatch(r"\t\{\s*(\d+), ([0-9.e+]+)\},", line)
        if not r:
            fail("%s: an envelope line that is not '\\t{<length>, <rms>},': %r" % (TABLE, line))
            continue
        env[int(r.group(1))] = float(r.group(2))
    return env


def planned(n):
    return max(PROBE_SQUARINGS, -(-PROBE_OUTPUTS // n))


def derive(rows):
    by = {}
    for n, p, d, mag, sq, outs, rms in rows:
        by.setdefault(n, []).append((p, d, mag, sq, outs, rms))
    want = lengths()
    if list(by) != want:
        fail("%s measures %d lengths, not the 144 supported ones in ascending order" % (DATA, len(by)))
    derived = {}
    for n in want:
        probes = by.get(n, [])
        if len(probes) < BACKOFF + 2:
            fail("length %d: fewer than %d probes" % (n, BACKOFF + 2))
            continue
        ps = [q[0] for q in probes]
        if ps != sorted(set(ps)):
            fail("length %d: probes not in strictly ascending order" % n)
        if held(probes[-1][1], probes[-1][2]):
            fail("length %d: the last probe holds; the scan must end at the first failure" % n)
        for p, d, mag, sq, outs, rms in probes:
            last = p == probes[-1][0]
            if not last and not held(d, mag):
                fail("length %d: probe %d fails before the last" % (n, p))
            if outs != sq * n:
                fail("length %d: probe %d counts %d outputs for %d squarings of %d words" % (n, p, outs, sq, n))
            if (not last and sq != planned(n)) or (last and not 1 <= sq <= planned(n)):
                fail("length %d: probe %d took %d squarings; the protocol takes %d (PLAN § 2.4)" % (n, p, sq, planned(n)))
        grid = []
        i = math.floor(probes[0][0] * STEPS / n) - 1
        while len(grid) < len(ps) + 2 and i < 10 ** 9:
            q = math.ceil(n * i / STEPS)
            if q >= max(3, n) and (not grid or q > grid[-1]):
                grid.append(q)
            i += 1
        if ps[0] in grid:
            k = grid.index(ps[0])
            if grid[k:k + len(ps)] != ps:
                fail("length %d: the probes are not consecutive grid exponents ceil(N i / %d)" % (n, STEPS))
        else:
            fail("length %d: the first probe %d is not a grid exponent" % (n, ps[0]))
        derived[n] = (ps[-2 - BACKOFF], probes[-2 - BACKOFF][5])
    return derived


def check():
    rows, _ = read_data()
    derived = derive(rows)
    table = read_table()
    if sorted(table) != lengths():
        fail("%s: the table's lengths are not the 144 supported lengths" % TABLE)
    env = read_envelope()
    if sorted(env) != lengths():
        fail("%s: the envelope's lengths are not the 144 supported lengths" % TABLE)
    for n in lengths():
        if n in derived and table.get(n) != derived[n][0]:
            fail("length %d: the table says %s, the measurement %d (R3)" % (n, table.get(n), derived[n][0]))
        if n in derived and env.get(n) != derived[n][1]:
            fail("length %d: the envelope says %s, the measurement %r" % (n, env.get(n), derived[n][1]))
    below = 2
    for n in lengths():
        top = table.get(n, 0)
        first = max(below + 1, n, 3)
        if first > top:
            fail("length %d is assigned no exponent: its largest, %d, is below %d" % (n, top, first))
        below = max(below, top)
    if table.get(1 << 20, 0) < P_FLOOR:
        fail("P_max %d is below 10,000,019 (R3)" % table.get(1 << 20, 0))


def sample(mode):
    rows, _ = read_data()
    by = {}
    for r in rows:
        by.setdefault(r[0], []).append(r[1])
    pick = QUICK if mode == "quick" else lengths()
    for n in pick:
        ps = by.get(n, [])
        for p in ps[-2:]:
            print(n, p)


def maxima(mode):
    # Each sample length's table maximum and the squarings the gate's own measurement takes there:
    # twice the protocol's, at least 128
    table = read_table()
    for n in (QUICK if mode == "quick" else lengths()):
        print(n, table[n], max(128, 2 * planned(n)))


def compare(path):
    _, text = read_data()
    got = [l for l in open(path).read().split("\n") if l]
    if not got:
        fail("%s: no probes measured again" % path)
    for line in got:
        f = line.split(" ")
        key = (int(f[0]), int(f[1]))
        if text.get(key) != line:
            fail("probe %d %d measured again as %r; %s says %r (R3, R9)" % (key[0], key[1], line, DATA, text.get(key)))


def table_rows():
    for n, p in sorted(read_table().items()):
        print(n, p)


cmd = sys.argv[1]
if cmd == "check":
    check()
elif cmd == "sample":
    sample(sys.argv[2])
elif cmd == "maxima":
    maxima(sys.argv[2])
elif cmd == "compare":
    compare(sys.argv[2])
elif cmd == "table":
    table_rows()
else:
    sys.exit("lengths.py: unknown command %s" % cmd)
for b in bad:
    print("lengths.py: " + b, file=sys.stderr)
sys.exit(1 if bad else 0)
