# tests/cli.sh against bcb6fdf's (a cycle record, not a standing gate). Run from the root of a built
# copy of the tree with python3 -I.
#
#   ll_cli.py check BASE_CLI REAL STANDIN
#
# BASE_CLI is bcb6fdf's tests/cli.sh; REAL the tree's bin/mps; STANDIN a bin/mps built from the tree
# whose test_lucas_lehmer is a stand-in chosen by GATE_LL: `stub` (bcb6fdf's: logs its start line and
# says it is not implemented yet), `reason` (another no verdict), `prime` and `error`. R16 as the
# GOAL words it: tests/cli.sh changes only in expectations whose outcome the Lucas-Lehmer test decides
# and in the record version. So:
#   (a) bcb6fdf's script, its record version alone made 2, passes whole against the stub: nothing
#       outside the Lucas-Lehmer test's results changed in bin/mps;
#   (b) every expectation of that script the real binary fails also fails against one of the other
#       stand-ins: it is one whose outcome the Lucas-Lehmer test decides;
#   (c) every expectation of that script the real binary passes is still in the tree's tests/cli.sh,
#       by name, as often, and passes there;
#   (d) in the text: bcb6fdf's helpers (run, run_all, run_stderr, has, lacks, once, expect) are in the
#       tree's script verbatim, and every `expect` line of bcb6fdf's script that the tree's no longer
#       holds verbatim names an expectation (b) found the Lucas-Lehmer test decides.
# The scripts run as copies in the tree's tests/ (they read ../src/main.hpp and ../AGENTS.md), with
# `expect` made to print ` PASS@<nonce> <name>` too, the nonce drawn by the gate for this run.

import collections
import os
import secrets
import re
import subprocess
import sys


NONCE = secrets.token_hex(8)
HELPERS = ("expect", "run", "run_all", "run_stderr", "has", "lacks", "once")


def instrument(text):
    marker = "\t\tpassed=$((passed + 1))\n"
    if text.count(marker) != 1:
        raise SystemExit("cli: tests/cli.sh's expect no longer counts a pass where the gate looks for it")
    return text.replace(marker, marker + '\t\techo " PASS@%s $name"\n' % NONCE)


def helper(text, name):
    m = re.search(r"^%s\(\) \{.*?^\}$" % re.escape(name), text, re.M | re.S)
    return m.group(0) if m else None


def template(line):
    m = re.match(r'\s*expect "((?:[^"\\]|\\.)*)"', line)
    if not m:
        return None
    parts = re.split(r'(\$\{?[A-Za-z_*][A-Za-z0-9_]*\}?|\$\*)', m.group(1))
    return re.compile("".join(".*" if p.startswith("$") else re.escape(p) for p in parts))


def run(script, binary, standin=None):
    env = {"PATH": "/usr/bin:/bin", "LC_ALL": "C"}
    if standin:
        env["GATE_LL"] = standin
    r = subprocess.run(["sh", script, binary], capture_output=True, text=True, env=env, timeout=1800)
    tag = " PASS@%s " % NONCE
    passed = collections.Counter(l[len(tag):] for l in r.stdout.splitlines() if l.startswith(tag))
    failed = collections.Counter(l[6:] for l in r.stdout.splitlines() if l.startswith(" FAIL "))
    summary = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
    return passed, failed, summary


def main(argv):
    base_path, real, standin = argv[2], argv[3], argv[4]
    bad = []
    base = open(base_path).read()
    if base.count('"record_version": 1}') != 1:
        bad.append("bcb6fdf's tests/cli.sh does not hold its record version once where the gate patches it")
    base_text = base.replace('"record_version": 1}', '"record_version": 2}')
    new_text = open("tests/cli.sh").read()
    base = instrument(base_text)
    new = instrument(new_text)
    open("tests/gate-base-cli.sh", "w").write(base)
    open("tests/gate-new-cli.sh", "w").write(new)
    try:
        sp, sf, ss = run("tests/gate-base-cli.sh", standin, "stub")
        if sf or not re.fullmatch(r"cli: \d+ passed, 0 failed", ss):
            bad.append("(a) bcb6fdf's tests/cli.sh, its record version alone made 2, fails against the stub: %s; %s"
                       % (ss, ", ".join(list(sf)[:5])))
        rp, rf, _ = run("tests/gate-base-cli.sh", real)
        decided = collections.Counter()
        for who in ("reason", "prime", "error"):
            _, f, _ = run("tests/gate-base-cli.sh", standin, who)
            decided |= f
        for name in rf:
            if name not in decided:
                bad.append("(b) the real binary fails '%s', whose outcome no stand-in for the Lucas-Lehmer test changes" % name)
        np_, nf, ns = run("tests/gate-new-cli.sh", real)
        if nf or not re.fullmatch(r"cli: \d+ passed, 0 failed", ns):
            bad.append("the tree's tests/cli.sh is red against the real binary: %s" % ns)
        for name, k in rp.items():
            if np_[name] < k:
                bad.append("(c) '%s' passes %d times in bcb6fdf's script against the real binary but %d times in the tree's"
                           % (name, k, np_[name]))
        for h in HELPERS:
            b = helper(base_text, h)
            if b is None or b not in new_text:
                bad.append("(d) bcb6fdf's helper %s() is not in the tree's tests/cli.sh verbatim" % h)
        kept = collections.Counter(new_text.split("\n"))
        for line in base_text.split("\n"):
            if kept[line] > 0:
                kept[line] -= 1
                continue
            t = template(line)
            if t is not None and not any(t.fullmatch(n) for n in rf):
                bad.append("(d) bcb6fdf's line '%s' is gone, and no expectation the test decides has its name" % line.strip()[:120])
    finally:
        for f in ("tests/gate-base-cli.sh", "tests/gate-new-cli.sh"):
            if os.path.exists(f):
                os.unlink(f)
    for b in bad[:30]:
        print("cli: " + b, file=sys.stderr)
    if len(bad) > 30:
        print("cli: and %d more" % (len(bad) - 30), file=sys.stderr)
    if not bad:
        print("cli: bcb6fdf's script green on the stub; %d expectations changed, each one the test decides; %d kept"
              % (sum(rf.values()), sum(rp.values())))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
