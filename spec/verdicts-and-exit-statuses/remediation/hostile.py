# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Remediation checker (review cycle 1, F1): judges remediation/hostile.cpp's output.
#   python3 -I hostile.py OUTPUT VERSION
# Every line is one line with no control byte (below 0x20, or DEL) and keeps its form; a plain
# reason stays as it is. Every record is one line of valid UTF-8 that parses as one JSON object,
# keeps its fields, and holds a valid-UTF-8 reason exactly.

import json
import sys

version = sys.argv[2]
rows = open(sys.argv[1]).read().split()
if not rows or len(rows) % 4:
    sys.exit("hostile: the probe printed %d lines, not four per reason" % len(rows))

suffix = (" (mps %s)" % version).encode()
verbatim = {b"plain reason, as a test would give it", "café, € and \U0001d11e".encode()}
bad = []
for i in range(0, len(rows), 4):
    reason, line, record, error = (bytes.fromhex(r) for r in rows[i:i + 4])
    for what, text, prefix in (("the no-verdict line", line, b"M31 tf: no verdict, "),
                               ("the error line", error, b"M31 ll: error, ")):
        if any(b < 0x20 or b == 0x7f for b in text):
            bad.append("%s for %r holds a control byte: %r" % (what, reason, text))
        if not text.startswith(prefix) or not text.endswith(suffix):
            bad.append("%s for %r lost its form: %r" % (what, reason, text))
        if reason in verbatim and text != prefix + reason + suffix:
            bad.append("%s for %r does not hold the reason as it is: %r" % (what, reason, text))
    if b"\n" in record or b"\r" in record:
        bad.append("the record for %r spans lines: %r" % (reason, record))
    try:
        obj = json.loads(record.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as err:
        bad.append("the record for %r is not one JSON object in valid UTF-8 (%s): %r" % (reason, err, record))
        continue
    if not isinstance(obj, dict):
        bad.append("the record for %r is not an object: %r" % (reason, record))
        continue
    for key, want in (("exponent", 31), ("worktype", "tf"), ("status", "no-verdict"),
                      ("program", {"name": "mps", "version": version}), ("record_version", 1)):
        if obj.get(key) != want:
            bad.append("the record for %r has %s %r, want %r" % (reason, key, obj.get(key), want))
    try:
        exact = reason.decode("utf-8")
    except UnicodeDecodeError:
        exact = None
    if not isinstance(obj.get("reason"), str):
        bad.append("the record for %r has no reason string: %r" % (reason, record))
    elif exact is not None and obj["reason"] != exact:
        bad.append("the record changed the valid reason %r into %r" % (reason, obj["reason"]))

for message in bad:
    print("hostile: " + message, file=sys.stderr)
sys.exit(1 if bad else 0)
