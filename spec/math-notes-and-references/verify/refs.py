# The online reference check for math-notes-and-references (a cycle record, not a standing gate).
# Python 3.8+, standard library only; needs the network.
#
#     python3 -I -B spec/math-notes-and-references/verify/refs.py            # check, compare the record
#     python3 -I -B spec/math-notes-and-references/verify/refs.py --write    # check, write the record
#
# For every doc/references.md entry with a DOI, it asks doi.org for the DOI's metadata (CSL JSON,
# by content negotiation, which Crossref and DataCite both answer) and compares the entry's title,
# authors, year and venue with it (R5). For an entry with a URL, it fetches the page and finds the
# entry's title in it. What it finds is written to evidence/references.txt, one line per entry,
# and the gate requires the committed record to match. A field the DOI's metadata lacks is
# reported `absent`, and that entry's Verified line must then name a second source by URL.
# Fetched pages and metadata are data: compared, never run.

import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import notes  # noqa: E402

RECORD = os.path.join(notes.EVIDENCE, "references.txt")
STOP = {"the", "of", "and", "in", "on", "a", "an", "for", "to", "de", "der", "und", "la", "le"}
AGENT = "mpsearch-cycle-gate/1 (reference check; python urllib)"


def fetch(url, accept):
	last = None
	for attempt in range(4):
		try:
			req = urllib.request.Request(url, headers={"Accept": accept, "User-Agent": AGENT})
			with urllib.request.urlopen(req, timeout=40) as r:
				return r.status, r.read(4_000_000).decode("utf-8", "replace")
		except urllib.error.HTTPError as e:
			if e.code in (404, 410):
				return e.code, ""
			last = "HTTP %d" % e.code
		except (urllib.error.URLError, TimeoutError, OSError) as e:
			last = str(e)
		time.sleep(2 * (attempt + 1))
	raise notes.Fail("could not reach %s: %s" % (url, last))


def years(csl):
	out = set()
	for k in ("issued", "published-print", "published-online", "published"):
		try:
			y = csl[k]["date-parts"][0][0]
			if y:
				out.add(int(y))
		except (KeyError, IndexError, TypeError, ValueError):
			pass
	return out


def words(s):
	return [w for w in notes.norm(s).split() if w not in STOP]


def family(name):
	return notes.squash(name.split()[-1]) if name.split() else ""


def initials(given):
	# the initials of a given name, "D. H." and "Derrick Henry" alike giving "dh"
	return "".join(w[0] for w in notes.norm(given or "").split())


# The kinds of work a reference may be; software, datasets and the like are not papers
TYPES = {"journal-article", "book", "book-chapter", "proceedings-article", "paper-conference", "article",
         "report", "monograph", "edited-book", "chapter", "standard", "reference-book"}


def page_text(url):
	status, body = fetch(url, "text/html,application/xhtml+xml,*/*;q=0.5")
	if status != 200:
		raise notes.Fail("%s answers HTTP %d" % (url, status))
	m = re.search(r"(?is)<title[^>]*>(.*?)</title>", body)
	title = re.sub(r"\s+", " ", m.group(1)) if m else ""
	text = re.sub(r"(?is)<(script|style)\b.*?</\1>", " ", body)
	text = re.sub(r"<[^>]+>", " ", text)
	return title, text


def page_meta(url):
	# the page's citation metadata (Highwire and Dublin Core tags), where it has any: the
	# years of its dates and the family names of its authors
	status, body = fetch(url, "text/html,application/xhtml+xml,*/*;q=0.5")
	years, people = set(), []
	for m in re.finditer(r"<meta\s[^>]*>", body, re.I):
		tag = m.group(0)
		name = re.search(r"\bname=[\"']([^\"']+)[\"']", tag, re.I)
		content = re.search(r"\bcontent=[\"']([^\"']*)[\"']", tag, re.I)
		if not name or not content:
			continue
		n, c = name.group(1).lower(), content.group(1)
		if n in ("citation_publication_date", "citation_date", "citation_online_date", "citation_cover_date",
		         "dc.date", "prism.publicationdate"):
			y = re.search(r"\b(1[5-9]\d\d|20\d\d)\b", c)
			if y:
				years.add(y.group(1))
		if n in ("citation_author", "dc.creator"):
			people.append(notes.squash(c.split(",")[0]) if "," in c else family(c))
	return years, sorted(people)


def second_source(e, need):
	# a field the DOI's metadata lacks is checked in the page the Verified line names
	m = re.search(r"https://[^\s,;)>]+", e["Verified"])
	if not m:
		raise notes.Fail("%s: doi:%s lacks %s; the Verified line must name a second source by URL" % (e["key"], e["doi"], need))
	url = m.group(0).rstrip(".:")
	try:
		_, text = page_text(url)
		years, people = page_meta(url)
	except notes.Fail as err:
		raise notes.Fail("%s: second source: %s" % (e["key"], err))
	t = notes.squash(text)
	ours = sorted(family(n) for n in re.split(r",\s*|\s+and\s+", e["Authors"]) if n.strip())
	for field in need:
		# where the page carries citation metadata, the field must equal it; otherwise it must
		# appear in the page
		if field == "authors" and people:
			if people != ours:
				raise notes.Fail("%s: the second source %s names %s" % (e["key"], url, people))
			continue
		if field == "year" and years:
			if e["Year"] not in years:
				raise notes.Fail("%s: the second source %s dates it %s" % (e["key"], url, sorted(years)))
			continue
		if field == "authors":
			for n in re.split(r",\s*|\s+and\s+", e["Authors"]):
				if n.strip() and family(n) not in t:
					raise notes.Fail("%s: the second source %s does not name %s" % (e["key"], url, n.strip()))
		if field == "year" and e["Year"] not in text:
			raise notes.Fail("%s: the second source %s does not give the year %s" % (e["key"], url, e["Year"]))
		if field == "title" and notes.squash(e["Title"]) not in t:
			raise notes.Fail("%s: the second source %s does not carry the title" % (e["key"], url))
	return url


def check_doi(e):
	key = e["key"]
	status, body = fetch("https://doi.org/" + e["doi"], "application/vnd.citationstyles.csl+json")
	if status != 200:
		raise notes.Fail("%s: doi:%s does not resolve (HTTP %d)" % (key, e["doi"], status))
	try:
		csl = json.loads(body)
	except ValueError:
		raise notes.Fail("%s: doi:%s returned no metadata" % (key, e["doi"]))
	out = {}
	kind = csl.get("type") or ""
	if kind not in TYPES and not (kind == "" and csl.get("container-title")):
		raise notes.Fail("%s: doi:%s is a %r, not a paper, book, chapter or standard" % (key, e["doi"], kind or "work of no type"))
	# Title: equal, ignoring case, spacing and punctuation; a subtitle may be separate in the
	# metadata, or (for a book) absent from it
	title = csl.get("title") or ""
	if isinstance(title, list):
		title = title[0] if title else ""
	title = re.sub(r"<[^>]+>", "", title)
	sub = csl.get("subtitle") or []
	sub = sub[0] if isinstance(sub, list) and sub else (sub if isinstance(sub, str) else "")
	t = notes.squash(e["Title"])
	if not title:
		out["title"] = "absent"
	elif t in (notes.squash(title), notes.squash(title + " " + sub)) or \
			(":" in e["Title"] and notes.squash(e["Title"].split(":")[0]) == notes.squash(title) and not sub):
		out["title"] = "ok"
	else:
		raise notes.Fail("%s: title %r; doi:%s has %r" % (key, e["Title"], e["doi"], (title + " " + sub).strip()))
	# Authors: the same family names
	people = csl.get("author") or []
	theirs = sorted(family(a.get("family") or a.get("literal") or "") for a in people)
	names = [n.strip() for n in re.split(r",\s*|\s+and\s+", e["Authors"]) if n.strip()]
	ours = sorted(family(n) for n in names)
	given = {family(a.get("family") or ""): initials(a.get("given")) for a in people if a.get("given")}
	if not theirs:
		out["authors"] = "absent"
	elif theirs == ours:
		for n in names:
			g = given.get(family(n))
			mine = initials(" ".join(n.split()[:-1]))
			if g and mine and not (g.startswith(mine) or mine.startswith(g)):
				raise notes.Fail("%s: author %s; doi:%s gives the initial %s" % (key, n, e["doi"], g.upper()))
		out["authors"] = "ok"
	else:
		raise notes.Fail("%s: authors %s; doi:%s has %s" % (key, ours, e["doi"], theirs))
	# Year: any date the metadata gives; or the year after its online or issued date, when the
	# Verified line names the print volume's year
	ys = years(csl)
	y = int(e["Year"])
	if not ys:
		out["year"] = "absent"
	elif y in ys:
		out["year"] = "ok"
	elif y == min(ys) + 1 and "published-print" not in csl and re.search(r"\bprint\b", e["Verified"]):
		out["year"] = "print year (metadata %d)" % min(ys)
	else:
		raise notes.Fail("%s: year %d; doi:%s gives %s" % (key, y, e["doi"], sorted(ys)))
	# Venue: at least half the significant words of the container (or, for a book or preprint,
	# the publisher) appear in the entry's venue
	cands = []
	for k in ("container-title", "collection-title", "publisher"):
		if k == "publisher" and cands:
			break    # the publisher stands for the venue only when there is no container
		v = csl.get(k)
		for s in (v if isinstance(v, list) else [v]):
			if isinstance(s, str) and s.strip():
				cands.append(s)
	venue = set(words(e["Venue"]))
	best = 0.0
	for c in cands:
		w = [x for x in words(c) if len(x) > 2 or x.isdigit()]
		if w:
			best = max(best, sum(1 for x in w if x in venue) / len(w))
	if not cands:
		out["venue"] = "absent"
	elif best >= 0.5:
		out["venue"] = "ok"
	else:
		raise notes.Fail("%s: venue %r; doi:%s has %s" % (key, e["Venue"], e["doi"], cands))
	need = [k for k in ("title", "authors", "year") if out[k] == "absent"]
	if need:
		url = second_source(e, need)
		for k in need:
			out[k] = "absent, found at %s" % url
	return "%s | doi:%s | %s | title %s | authors %s | year %s | venue %s" % (
		key, e["doi"], kind or "-", out["title"], out["authors"], out["year"], out["venue"])


def check_url(e):
	# a page: its title (in the page's <title> when the title is three words or fewer), each
	# author's family name and the year must all appear in it
	key = e["key"]
	try:
		title, text = page_text(e["url"])
	except notes.Fail as err:
		raise notes.Fail("%s: %s" % (key, err))
	where = title if len(e["Title"].split()) <= 3 else text
	if notes.squash(e["Title"]) not in notes.squash(where):
		raise notes.Fail("%s: %s does not carry the title %r%s" % (key, e["url"], e["Title"],
		                 " in its <title>" if where is title else ""))
	t = notes.squash(text)
	for n in re.split(r",\s*|\s+and\s+", e["Authors"]):
		if n.strip() and (family(n) if "." in n else notes.squash(n)) not in t:
			raise notes.Fail("%s: %s does not name %s" % (key, e["url"], n.strip()))
	if e["Year"] not in text:
		raise notes.Fail("%s: %s does not give the year %s" % (key, e["url"], e["Year"]))
	return "%s | url:%s | http 200 | title, authors and year found" % (key, e["url"])


def main(argv):
	import datetime
	try:
		entries = notes.parse_references(datetime.date.today().isoformat())
		lines = ["# Reference check for %s, written by: python3 -I -B %s/verify/refs.py --write" % (notes.SLUG, notes.SPEC),
		         "# Each DOI resolved at doi.org; its metadata was compared with the entry. Each URL answered 200 and carries the title.", ""]
		for key in sorted(entries, key=str.lower):
			e = entries[key]
			lines.append(check_doi(e) if "doi" in e else check_url(e))
		got = "\n".join(lines) + "\n"
		if len(argv) == 2 and argv[1] == "--write":
			os.makedirs(notes.EVIDENCE, exist_ok=True)
			with open(RECORD, "w", encoding="utf-8") as f:
				f.write(got)
		elif not os.path.isfile(RECORD) or open(RECORD, encoding="utf-8").read() != got:
			raise notes.Fail("%s/evidence/references.txt differs from what the check finds; rerun refs.py --write and commit it"
			                 % notes.SPEC)
		print("refs gate: green (%d entries)" % len(entries))
		return 0
	except notes.Fail as e:
		print("GATE FAIL: %s" % e, file=sys.stderr)
		return 1


if __name__ == "__main__":
	sys.exit(main(sys.argv))
