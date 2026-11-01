# The checks' log, as the gates read it (a cycle record, not a standing gate). Run with python3 -I.
#
#   ll_checklog.py failed LOG LABEL...     every LABEL ran and failed (" FAIL <label>" printed)
#   ll_checklog.py disagree LOG LABEL...   every LABEL printed at least one tally line, and on each of
#                                          its tally lines every case disagreed: "   tally <what>: K
#                                          cases, K disagree" with K > 0 (PLAN § 2.8: a check that
#                                          calls a case but grades only part of it leaves W < K)
#   ll_checklog.py passed LOG LABEL...     every LABEL ran and did not fail
#   ll_checklog.py tally LOG LABEL WHAT    LABEL printed at least one tally line whose <what> starts
#                                          with WHAT, and on each of them every case disagreed
#
# bin/check prints "check <label>" before each check's output and " FAIL <label>" after a failure.

import re
import sys


def sections(text):
    out, label = {}, None
    for line in text.splitlines():
        if line.startswith("check "):
            label = line[len("check "):]
            out.setdefault(label, [])
        elif label is not None:
            out[label].append(line)
    return out


def main(argv):
    mode, log, labels = argv[1], argv[2], argv[3:]
    prefix = None
    if mode == "tally":
        mode, labels, prefix = "disagree", [argv[3]], argv[4]
    text = open(log, errors="replace").read()
    secs = sections(text)
    failed = set(l[len(" FAIL "):] for l in text.splitlines() if l.startswith(" FAIL "))
    bad = []
    for label in labels:
        if label not in secs:
            bad.append("'%s' did not run" % label)
            continue
        if mode == "failed" and label not in failed:
            bad.append("'%s' did not fail" % label)
        elif mode == "passed" and label in failed:
            bad.append("'%s' failed" % label)
        elif mode == "disagree":
            tallies = [re.fullmatch(r"   tally (.+): (\d+) cases, (\d+) disagree", l) for l in secs[label]]
            tallies = [t for t in tallies if t and (prefix is None or t.group(1).startswith(prefix))]
            if not tallies:
                bad.append("'%s' printed no tally line" % label)
            for t in tallies:
                k, w = int(t.group(2)), int(t.group(3))
                if k == 0 or w != k:
                    bad.append("'%s': tally %s: %d cases, %d disagree (want every case to disagree)" % (label, t.group(1), k, w))
    for b in bad:
        print("checklog: " + b, file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
