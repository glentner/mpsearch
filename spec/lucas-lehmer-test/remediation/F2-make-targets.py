# Review cycle 1, F2 (a cycle record, not a standing gate): every profile row and every § Build and
# run line of AGENTS.md that describes a make target says what that target runs, by the Makefile's
# recipes.
#
#   python3 -I spec/lucas-lehmer-test/remediation/F2-make-targets.py [ROOT]
#
# The steps of each target are read from its recipe (bin/check, bin/check engine, tests/cli.sh,
# tests/ll.sh, tests/ll.sh long), and each must be named in every description of the target: the
# profile row whose value starts with the target's command, and the target's comment in the
# § Build and run block (`make check's three steps` names make check's steps when it counts them
# right). A description that counts the steps (both, two, three) counts them right, and one that
# says they always run is only true of a recipe that runs each step whatever the last one did.
# Prints what is wrong and exits 1, or prints one line and exits 0.

import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
agents = open(os.path.join(root, "AGENTS.md")).read()
makefile = open(os.path.join(root, "Makefile")).read()
bad = []

# What each step is called in prose, any one of the names
NAMES = {"bin/check": ["bin/check", "the checks"], "bin/check engine": ["bin/check engine", "engine tier"],
         "tests/cli.sh": ["tests/cli.sh", "smoke test"], "tests/ll.sh": ["tests/ll.sh"],
         "tests/ll.sh long": ["tests/ll.sh long"]}
COUNTS = {"both": 2, "two": 2, "three": 3, "four": 4, "five": 5}


def recipe(target):
    m = re.search(r"^%s:[^\n]*\n((?:\t[^\n]*\n)+)" % re.escape(target), makefile, re.M)
    return m.group(1).replace("\\\n", " ") if m else None


def steps(target):
    r = recipe(target)
    if r is None:
        return None, False
    found = []
    for m in re.finditer(r"\$\(CHECK\)( engine)?|sh tests/(cli|ll)\.sh \$\(EXE\)( long)?", r):
        if m.group(0).startswith("$(CHECK)"):
            found.append("bin/check engine" if m.group(1) else "bin/check")
        else:
            found.append("tests/%s.sh%s" % (m.group(2), " long" if m.group(3) else ""))
    always = len(found) > 1 and r.count("|| status=1") == len(found)
    return found, always


TARGETS = {}
for t in re.findall(r"^\.PHONY:(.*)$", makefile, re.M)[0].split():
    s, always = steps(t)
    if s:
        TARGETS[t] = (s, always)
if not {"check", "check-engine", "check-long"} <= set(TARGETS):
    bad.append("the Makefile's check, check-engine and check-long recipes run none of the known steps: %s" % sorted(TARGETS))


def grade(where, target, text):
    want, always = TARGETS[target]
    low = text.lower()
    covered = set()
    m = re.search(r"make (\S+?)'s (\w+) steps", text)
    if m and m.group(1) in TARGETS:
        inner = TARGETS[m.group(1)][0]
        if COUNTS.get(m.group(2)) != len(inner):
            bad.append("%s: says make %s has %s steps; its recipe runs %d" % (where, m.group(1), m.group(2), len(inner)))
        covered |= set(inner)
    for s in want:
        if s in covered:
            continue
        names = NAMES[s]
        if s == "tests/ll.sh" and "tests/ll.sh long" not in want:
            names = names + ["ll.sh"]
        if not any(n in text for n in names):
            bad.append("%s: describes make %s without naming %s, which its recipe runs" % (where, target, s))
    for word, n in COUNTS.items():
        if re.search(r"\b%s\b" % word, low) and not (m and m.group(2) == word) and n != len(want):
            bad.append("%s: says '%s' of make %s, whose recipe runs %d steps" % (where, word, target, len(want)))
    if "always run" in low and not always:
        bad.append("%s: says make %s's steps always run; its recipe does not run each whatever the last did" % (where, target))


# The profile's rows whose value starts with a make target's command
graded = 0
for line in agents.split("\n"):
    m = re.match(r"^\| ([\w ]+) \| `make ([\w-]+)`(.*)\|$", line)
    if m and m.group(2) in TARGETS:
        grade("the profile's %s row" % m.group(1), m.group(2), m.group(3))
        graded += 1

# § Build and run: each `make TARGET` line of the code block with its comment and continuation lines
br = agents[agents.find("## Build and run"):agents.find("## Map")]
block = br[br.find("```sh"):br.find("```", br.find("```sh") + 5)]
current, comment = None, []
entries = []
for line in block.split("\n")[1:]:
    m = re.match(r"^make ([\w-]+)\s+# (.*)$", line)
    if m:
        if current:
            entries.append((current, " ".join(comment)))
        current, comment = m.group(1), [m.group(2)]
    elif current and re.match(r"^\s+# ", line):
        comment.append(line.split("# ", 1)[1])
    else:
        if current:
            entries.append((current, " ".join(comment)))
        current, comment = None, []
for target, text in entries:
    if target in TARGETS:
        grade("§ Build and run's make %s" % target, target, text)
        graded += 1

if graded < 4:
    bad.append("only %d descriptions of make check, check-engine and check-long found in AGENTS.md, want the verify row and three § Build and run lines" % graded)
for b in bad:
    print("F2: " + b)
if bad:
    sys.exit(1)
print("F2: %d descriptions of make targets name every step their recipes run" % graded)
