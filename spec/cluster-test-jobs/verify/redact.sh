#!/bin/sh
# Redact one piece of cluster evidence before it is committed (P4). Usage:
#   sh spec/cluster-test-jobs/verify/redact.sh IN OUT
# A result.json (IN ending in .json) gets "cluster": "<cluster>" and "host": "<host>"; then, in any
# file, the site's scratch paths become <scratch>, its other site paths <site-path>, and its account,
# user and cluster names <site>, all read from ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}. The
# public repository URL is left alone. Fails if the site file is missing or incomplete.
set -u
. "$(dirname "$0")/common.sh"
need python3
[ $# -eq 2 ] || fail "usage: sh redact.sh IN OUT"
test -f "$1" || fail "no such file: $1"
cat >"$work/redact.py" <<'EOF'
import json, re, sys

site, url, src, dst = sys.argv[1:5]
vals = {}
try:
    lines = open(site).read().splitlines()
except OSError as e:
    sys.exit("site file %s: %s" % (site, e))
for line in lines:
    m = re.fullmatch(r'\s*([A-Z_]+)=(?:"([^"]*)"|\'([^\']*)\'|(\S*))\s*', line)
    if m:
        vals[m.group(1)] = (m.group(2) or m.group(3) or m.group(4) or "").split()
for k in ["MPS_SITE_ACCOUNTS", "MPS_SITE_USER", "MPS_SITE_CLUSTERS", "MPS_SITE_SCRATCH_PREFIX", "MPS_SITE_PATH_PREFIXES"]:
    if not vals.get(k):
        sys.exit("site file %s: %s is missing or empty" % (site, k))
text = open(src).read()
if src.endswith(".json"):
    r = json.loads(text)
    if r.get("cluster") is not None:
        r["cluster"] = "<cluster>"
    r["host"] = "<host>"
    text = json.dumps(r, indent=2) + "\n"
keep = "\0URL\0"
text = re.sub(re.escape(url), keep, text, flags=re.I)
for p in vals["MPS_SITE_SCRATCH_PREFIX"]:
    text = re.sub(re.escape(p) + r"[^\s'\"]*", "<scratch>", text, flags=re.I)
for p in vals["MPS_SITE_PATH_PREFIXES"]:
    text = re.sub(re.escape(p) + r"[^\s'\"]*", "<site-path>", text, flags=re.I)
words = vals["MPS_SITE_ACCOUNTS"] + vals["MPS_SITE_USER"] + vals["MPS_SITE_CLUSTERS"]
text = re.sub(r"(?<![A-Za-z0-9])(%s)(?![A-Za-z0-9])" % "|".join(map(re.escape, words)), "<site>", text, flags=re.I)
text = text.replace(keep, url)
open(dst, "w").write(text)
EOF
python3 -I "$work/redact.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$1" "$2" || fail "redaction failed"
scan_site_private "$2"
echo "redacted: $2"
