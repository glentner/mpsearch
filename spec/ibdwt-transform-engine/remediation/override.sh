# Foreman ruling (2026-10-31, remediation/make.sh): the file check grades what changed as of phase 4.
_fc=$(sed -n '/^files_check() {$/,/^}$/p' "$here/gate.sh" | sed 's/files\.py" changes "\$PHASE"/files.py" changes 4/')
case $_fc in *'files.py" changes 4 '*) eval "$_fc" ;; *) die "remediation: files_check not found in gate.sh" ;; esac
sh "$root/$spec/remediation/make.sh" check >/dev/null || die "remediation: the scripts drifted from verify/ (make.sh check)"
