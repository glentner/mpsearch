#!/bin/sh
# Redact one piece of cluster evidence before it is committed (P5; copied from
# spec/logger-state-per-translation-unit, the redaction itself in cluster.sh). Usage:
#   sh spec/reference-big-integer/verify/redact.sh IN OUT
# A result.json (IN ending in .json) gets "cluster": "<cluster>" and "host": "<host>"; then, in any
# file, the host after a detailed stamp, plain or coloured, becomes <host>, the site's scratch paths
# <scratch>, its other site paths <site-path>, and its account, user and cluster names <site>, all
# read from ${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}. The public repository URL is left
# alone. OUT is then scanned; fails if the site file is missing or incomplete, or OUT is not clean.
set -u
. "$(dirname "$0")/common.sh"
. "$(dirname "$0")/cluster.sh"
[ $# -eq 2 ] || fail "usage: sh redact.sh IN OUT"
test -f "$1" || fail "no such file: $1"
redact "$1" "$2"
echo "redacted: $2"
