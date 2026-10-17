# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Build outputs for this cycle's gates (a cycle record, not a standing gate)
#
# Run as `python3 -I -B outputs.py COMMAND ...` from common.sh. Every command reads or dates files
# under a copy's lib/ and bin/, or reads a gate's own logs; none deletes anything.
#
#   manifest DIR OUT           every non-directory under DIR/lib and DIR/bin: its path, type, size,
#                              inode, mtime (ns) and SHA-256, as JSON. A 64-bit Mach-O file is
#                              hashed with its LC_UUID and its code signature zeroed: Apple's
#                              linker gives an -O0 link a fresh UUID each time (measured), and the
#                              ad-hoc signature hashes the page holding it; every other byte counts
#   same SCOPE WANT GOT        GOT holds every file of WANT in SCOPE, byte for byte (SCOPE: mps is
#                              WANT's lib/*.o and bin/mps; all is every lib/**.o and both binaries)
#   differs SCOPE A B          A and B differ in SCOPE's binaries, so comparing with either can tell
#                              them apart: mps needs bin/mps to differ, check bin/check, all both
#   unchanged A B              A and B are the same files, untouched: path, inode, mtime, size, hash
#   age DIR SECONDS            date every file under DIR/lib and DIR/bin SECONDS from now
#   date FILE SECONDS          date FILE SECONDS from now
#   calls LOG [CXX]            the -o targets of the compiler calls in LOG, one per line, sorted; with
#                              CXX, fail if any call in LOG is by another compiler
#   commands OUT CXX           make's own command lines in OUT (those starting `CXX ` or `mkdir `)
#   summaries OUT              the check and cli summary lines in OUT, which must both be there and
#                              say 0 failed

import hashlib, json, os, re, stat, struct, sys, time


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def digest(data):
    if data[:4] == b"\xcf\xfa\xed\xfe":
        buf = bytearray(data)
        ncmds = struct.unpack_from("<I", buf, 16)[0]
        off = 32
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", buf, off)
            if size < 8 or off + size > len(buf):
                die("a malformed Mach-O load command")
            if cmd == 0x1B:                                   # LC_UUID
                buf[off + 8:off + 24] = bytes(16)
            elif cmd == 0x1D:                                 # LC_CODE_SIGNATURE
                dataoff, datasize = struct.unpack_from("<II", buf, off + 8)
                buf[dataoff:dataoff + datasize] = bytes(datasize)
            off += size
        data = bytes(buf)
    return hashlib.sha256(data).hexdigest()


def manifest(root, out):
    files = {}
    for top in ("lib", "bin"):
        base = os.path.join(root, top)
        if not os.path.lexists(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames.sort()
            for name in sorted(filenames) + [d for d in dirnames if os.path.islink(os.path.join(dirpath, d))]:
                p = os.path.join(dirpath, name)
                st = os.lstat(p)
                rel = os.path.relpath(p, root)
                kind = "file" if stat.S_ISREG(st.st_mode) else "link" if stat.S_ISLNK(st.st_mode) else "other"
                if kind == "file":
                    with open(p, "rb") as f:
                        h = digest(f.read())
                else:
                    h = None
                files[rel] = {"kind": kind, "size": st.st_size, "inode": st.st_ino,
                              "mtime": st.st_mtime_ns, "sha256": h}
    with open(out, "w") as f:
        json.dump(files, f, indent=1, sort_keys=True)


def load(p):
    with open(p) as f:
        return json.load(f)


def scope_files(scope, want):
    if scope == "mps":
        names = [p for p in want if re.fullmatch(r"lib/[^/]+\.o", p)] + ["bin/mps"]
    elif scope == "all":
        names = [p for p in want if re.fullmatch(r"lib/(check/)?[^/]+\.o", p)] + ["bin/mps", "bin/check"]
    else:
        die("unknown scope %s" % scope)
    return sorted(names)


def same(scope, want_path, got_path):
    want, got = load(want_path), load(got_path)
    names = scope_files(scope, want)
    if len(names) < 3:
        die("the reference %s holds only %s: no build to compare against" % (want_path, names))
    bad = []
    for p in names:
        if p not in want:
            die("the reference %s has no %s" % (want_path, p))
        if p not in got:
            bad.append("%s missing" % p)
        elif got[p]["sha256"] != want[p]["sha256"]:
            bad.append("%s differs" % p)
    if bad:
        die("%d of %d files are not what a fresh build with these settings makes: %s"
            % (len(bad), len(names), ", ".join(bad[:6]) + (" ..." if len(bad) > 6 else "")))


def differs(scope, a_path, b_path):
    a, b = load(a_path), load(b_path)
    names = {"mps": ["bin/mps"], "check": ["bin/check"], "all": ["bin/mps", "bin/check"]}[scope]
    for name in names:
        if name not in a or name not in b:
            die("%s missing from a reference" % name)
        if a[name]["sha256"] == b[name]["sha256"]:
            die("%s is the same under both settings: the comparison could not tell them apart" % name)


def unchanged(a_path, b_path):
    a, b = load(a_path), load(b_path)
    if not a:
        die("nothing under lib/ and bin/ before the run: a no-op check needs a built tree")
    if a != b:
        changed = sorted(p for p in set(a) | set(b) if a.get(p) != b.get(p))
        die("the run changed %d files under lib/ and bin/: %s" % (len(changed), ", ".join(changed[:6])))


def age(root, seconds):
    t = time.time() + float(seconds)
    n = 0
    for top in ("lib", "bin"):
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
            for name in filenames:
                os.utime(os.path.join(dirpath, name), (t, t))
                n += 1
    if n == 0:
        die("nothing under %s/lib and bin to date" % root)


def date(p, seconds):
    t = time.time() + float(seconds)
    os.utime(p, (t, t))


def calls(log, cxx=None):
    targets = []
    if os.path.exists(log):
        for line in open(log):
            words = line.split()
            if not words:
                continue
            if cxx is not None and words[0] != cxx:
                die("a compiler call by %s in a run with CXX=%s: %s" % (words[0], cxx, line.strip()[:160]))
            if "-o" in words and words.index("-o") + 1 < len(words):
                targets.append(words[words.index("-o") + 1])
    for t in sorted(targets):
        print(t)


def commands(out, cxx):
    for line in open(out):
        if line.startswith(cxx + " ") or line.startswith("mkdir "):
            print(line.rstrip("\n"))


def summaries(out):
    text = open(out).read()
    c = re.findall(r"^check: [0-9]+ passed, 0 failed$", text, re.M)
    l = re.findall(r"^cli: [0-9]+ passed, 0 failed$", text, re.M)
    if len(c) != 1 or len(l) != 1:
        die("make check did not print one passing check summary and one passing cli summary")
    print(c[0])
    print(l[0])


cmd, args = sys.argv[1], sys.argv[2:]
{"manifest": manifest, "same": same, "differs": differs, "unchanged": unchanged, "age": age,
 "date": date, "calls": calls, "commands": commands, "summaries": summaries}[cmd](*args)
