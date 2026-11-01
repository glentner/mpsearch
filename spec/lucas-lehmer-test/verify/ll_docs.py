# The notes (a cycle record, not a standing gate). Run from the repository root with python3 -I.
#
#   ll_docs.py PHASE          R18's notes as of the phase: P1, the Jacobi symbol written up in
#                             doc/reference-arithmetic.md, naming `jacobi`, cited, the citation in
#                             doc/references.md; P2, doc/lucas-lehmer.md's "What mps uses" as built:
#                             the names PLAN § 2.7 requires, each limit as "`NAME` (value)" with the value
#                             src/lucas.hpp gives it, no stale sentence anywhere in the note; P3, no note
#                             says make check runs the reference tests "on the published Mersenne prime
#                             exponents"
#   ll_docs.py claims PHASE   spec/math-notes-and-references/verify/notes.py claims on every note this
#                             cycle changed (doc/README.md's markers)

import re
import subprocess
import sys

CITES = ("ShallitSorenson1993", "BrentZimmermann2010")
REQUIRED = ["`lucas::test`", "`src/lucas.cpp`", "`test_lucas_lehmer`", "shift", "Jacobi", "degenerate", "confirm",
            "replay", "larger", "length", "0.35", "largest distance", "prime exponents", "P_max"]
LIMITS = ["GOOD_STATES", "CHECK_FAILURES", "LENGTH_RETRIES", "PROGRESS_SECONDS", "DISTANCES_LOGGED"]
STALE = ["not implemented yet", "is a stub", "still a stub", "planned test", "The planned", "will be graded",
         "unless the GCD gives a factor", "It is not wired into `-t`"]
PUBLISHED = "on the published Mersenne prime exponents"


def section(text, title):
    m = re.search(r"^## %s\s*$" % re.escape(title), text, re.M)
    if not m:
        return None
    rest = text[m.end():]
    n = re.search(r"^## ", rest, re.M)
    return rest[:n.start()] if n else rest


def value(header, name):
    m = re.search(r"static const (?:std::uint64_t|double) %s = ([0-9.']+);" % name, header)
    return m.group(1).replace("'", "") if m else None


def main(argv):
    bad = []
    if argv[1] == "claims":
        phase = int(argv[2])
        changed = subprocess.run(["git", "diff", "--name-only", "bcb6fdf", "--", "doc"], capture_output=True, text=True).stdout.split()
        changed += subprocess.run(["git", "ls-files", "--others", "--exclude-standard", "doc"], capture_output=True, text=True).stdout.split()
        notes = sorted(set(f for f in changed if f.endswith(".md") and f not in ("doc/README.md", "doc/references.md")))
        if notes:
            r = subprocess.run([sys.executable, "-I", "-B", "spec/math-notes-and-references/verify/notes.py", "claims"] + notes,
                               capture_output=True, text=True)
            sys.stdout.write(r.stdout + r.stderr)
            return r.returncode
        return 0
    phase = int(argv[1])
    ra = open("doc/reference-arithmetic.md").read()
    refs = open("doc/references.md").read()
    if "Jacobi" not in ra or "`jacobi`" not in ra:
        bad.append("doc/reference-arithmetic.md does not write up the Jacobi symbol and name `jacobi` (R18)")
    cited = [c for c in CITES if c in ra]
    if not cited:
        bad.append("doc/reference-arithmetic.md cites neither %s for the Jacobi symbol's algorithm (R18)" % " nor ".join(CITES))
    for c in cited:
        if not re.search(r"^#+ .*%s|<a id=\"%s\"|\(#%s\)|^- \*\*%s\*\*|id=\"%s\"" % ((c.lower(),) * 2 + (c.lower(),) + (c,) + (c.lower(),)), refs, re.M | re.I) \
                and c not in refs:
            bad.append("doc/references.md has no entry %s" % c)
    if phase >= 2:
        note = open("doc/lucas-lehmer.md").read()
        uses = section(note, "What mps uses")
        header = open("src/lucas.hpp").read()
        if uses is None:
            bad.append("doc/lucas-lehmer.md has no '## What mps uses'")
        else:
            for r in REQUIRED:
                if r not in uses:
                    bad.append("doc/lucas-lehmer.md's 'What mps uses' does not name %s (R18)" % r)
            for name in LIMITS:
                v = value(header, name)
                ms = re.findall(r"`%s` \(([0-9.]+)( s)?\)" % name, note)
                if v is None:
                    bad.append("src/lucas.hpp does not set %s" % name)
                elif not re.search(r"`%s` \(" % name, uses):
                    bad.append("doc/lucas-lehmer.md's 'What mps uses' does not state `%s` (%s) (R18)" % (name, v))
                for got, _ in ms:
                    if float(got) != float(v):
                        bad.append("doc/lucas-lehmer.md says `%s` (%s), src/lucas.hpp says %s" % (name, got, v))
        for s in STALE:
            if s in note:
                bad.append("doc/lucas-lehmer.md still says '%s' (R18)" % s)
    if phase >= 3:
        for f in ("doc/lucas-lehmer.md", "doc/prp-and-gerbicz.md", "doc/reference-arithmetic.md"):
            text = open(f).read()
            if PUBLISHED in text:
                bad.append("%s still says make check runs the reference tests '%s' (R18)" % (f, PUBLISHED))
            if "tests/known/mersenne-exponents.txt" not in text or "tests/vectors/reference.txt" not in text:
                bad.append("%s does not say what make check's reference tests run: tests/known/mersenne-exponents.txt and tests/vectors/reference.txt (R18)" % f)
    for b in bad:
        print("docs: " + b, file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
