# Review cycle 1, F4 (a cycle record, not a standing gate): no seed under issues/, and no entry of
# ROADMAP.md, cites a line number in a file this cycle changed. The cycle moves such a line; a seed
# names the function, the target or the section instead.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F4-line-refs.py [ROOT]
#
# The files this cycle changed are what `git diff --name-only bcb6fdf` and the untracked files
# list, spec/ aside. A line reference is `path:N`, `path:N-M`, a following `:N` that continues it,
# or a path or file name followed by "line N" or "(line N"; a file name matches a changed path by
# its last component. Prints each reference and exits 1, or prints one line and exits 0.

import glob
import os
import re
import subprocess
import sys

BASE = "bcb6fdf"
root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))


def git(*args):
    return subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True, check=True).stdout.split("\n")


changed = {p for p in git("diff", "--name-only", BASE) + git("ls-files", "--others", "--exclude-standard")
           if p and not p.startswith("spec/")}
if not changed:
    sys.exit("F4: nothing changed since %s: not a tree of this cycle" % BASE)
by_name = {}
for p in changed:
    by_name.setdefault(os.path.basename(p), set()).add(p)

PATH = r"[A-Za-z0-9_][A-Za-z0-9_./-]*"
COLON = re.compile(r"(%s):(\d+)(?:-\d+)?" % PATH)
CONT = re.compile(r"`:(\d+)(?:-\d+)?`")
WORDS = re.compile(r"`(%s)`[)'’s]*\s*\(?lines?\s+(\d+)" % PATH)

files = sorted(glob.glob(os.path.join(root, "issues", "*.md"))) + [os.path.join(root, "ROADMAP.md")]
found = []
scanned = 0
for f in files:
    if not os.path.exists(f):
        continue
    scanned += 1
    rel = os.path.relpath(f, root)
    text = open(f).read()
    flat = re.sub(r"\s+", " ", text)
    refs = []
    last_path = None
    for m in re.finditer(r"%s|%s" % (COLON.pattern, CONT.pattern), flat):
        if m.group(1):
            last_path = m.group(1)
            refs.append((m.group(1), m.group(2)))
        elif last_path:
            refs.append((last_path, m.group(3)))
    for m in WORDS.finditer(flat):
        refs.append((m.group(1), m.group(2)))
    for target, line in refs:
        if target.startswith(("http", "doi", "arXiv")):
            continue
        hits = ({target} & changed) if "/" in target else by_name.get(target, set())
        if hits:
            found.append("%s cites line %s of %s, which this cycle changed: name the function, target or section" % (rel, line, sorted(hits)[0]))

for b in found:
    print("F4: " + b)
if found:
    sys.exit(1)
print("F4: %d seed and roadmap files cite no line of the %d files this cycle changed" % (scanned, len(changed)))
