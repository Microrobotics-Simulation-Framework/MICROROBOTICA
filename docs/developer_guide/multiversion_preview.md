---
orphan: false
---

# Multi-version docs: local preview

```{note}
This page documents the **preview** workflow on the
`feat/versioned-docs-preview` branch.  The full release plan is in
[MADDENING/docs/developer_guide/versioned_docs.md](https://microrobotica.org/maddening/developer_guide/versioned_docs.html).
The version switcher is **off by default** — CI keeps deploying the
single-version site.  To preview the multi-version layout locally,
opt in with `DOCS_MULTIVERSION=1` (the Makefile targets below do this
for you).
```

## Quick start

```bash
cd MICROROBOTICA/docs

# One-shot build of every version + serve locally
make preview-versions     # builds, then serves _build/html on :8000

# Open http://localhost:8000/maddening/ in a browser.
# The version switcher is in the top navbar.
```

The server is `scripts/pages_server.py`, which resolves a URL the way
GitHub Pages does (`/x` serves `x.html`, `/dir` redirects to `/dir/`,
`/x.html/` is a 404; its docstring lists each rule with what
microrobotica.org answered).  `python3 -m http.server` does not serve
`x.html` for `/x`, so the extensionless links MIME's errors print
cannot be checked with it.

`sphinx-build` must be reachable.  The Makefile auto-detects the
sibling shared venv (`../../.venv`); otherwise activate your docs
venv first (`source ../../.venv/bin/activate`) or pass an explicit
`make … SPHINXBUILD=/path/to/sphinx-build`.

The output structure mirrors the production target:

```
_build/html/
├── index.html                       ← MICROROBOTICA latest
├── _static/switcher.json            ← MICROROBOTICA version list
├── sitemap.xml                      ← MICROROBOTICA sitemap
├── maddening/
│   ├── index.html                   ← MADDENING latest (= main tip)
│   ├── _static/switcher.json        ← canonical MADDENING version list
│   ├── sitemap.xml                  ← MADDENING sitemap (latest only)
│   ├── v0.1/index.html              ← MADDENING v0.1.0 tag
│   └── v0.2/index.html              ← MADDENING v0.2.0 tag
└── mime/
    ├── index.html                   ← MIME latest (= the 0.3.0 pages)
    ├── _static/switcher.json        ← MIME version list (docs/_switchers/mime/)
    ├── 0.3.0/index.html             ← MIME 0.3.0 release pages
    ├── v0.1/index.html              ← MIME v0.1.0 tag
    └── v0.2/index.html              ← MIME v0.2.0 tag
```

Every version's HTML references `/maddening/_static/switcher.json` via
a root-relative URL, so v0.1 pages pick up the switcher dropdown even
though v0.1's source tree predates the switcher.

## MIME 0.3 on: the release's pages

MIME's documentation is generated from 0.3 on: `mime docs build --out
<dir>` writes `index.md` and `pages/` from a MIME checkout and its
installed registry, and no git ref's `docs/` holds an `index.md`.  So
MIME 0.3 is not built from a worktree:

* `MIME_RELEASES` in `docs/Makefile` lists `label:tag` pairs
  (`0.3.0:v0.3.0.dev1`).  `make mime-pages` downloads each release's
  asset, `https://github.com/Microrobotics-Simulation-Framework/MIME/releases/download/<tag>/mime-docs-<version>.tar.gz`
  (`<version>` is the tag without its `v`), unpacks it into
  `_build/mime-pages/<label>/` (ignored by git; never commit MIME's
  pages here) and builds it to `/mime/<label>/`.  The label is the
  release number with no `v`, because MIME's errors link to
  `https://microrobotica.org/mime/<release>/pages/errors/<code>`.
* `MIME_LATEST` is built a second time to `/mime/`, and the switcher
  marks it as that release (`DOCS_SWITCHER_MATCH`).  MIME's `main` is
  the 0.3 line from its release on, so `latest` no longer follows it.
* The switcher list is `docs/_switchers/mime/switcher.json`, here,
  since the generated pages have no `_static/`.
* **Fallback** (no asset yet, or to read a change before it is
  released): in a MIME checkout whose environment has MIME's `verify`
  extra, `uv run --no-sync mime docs build --out <dir>`, then
  `make all-versions MIME_PAGES_DIR=<dir>`.  Such pages print every
  evidence count as "not read" (no lane report); the release asset,
  built in MIME's CI, reads them.
* `make check-mime-docs` (a step of the docs workflow): every code in
  MIME's catalogue (`pages/errors/index.md`) has its page under
  `/mime/<release>/` and `/mime/`, its extensionless URL answers with
  it under GitHub Pages' rules, and every link of the release's pages
  lands, anchor included.  After a deploy, the workflow samples the
  same extensionless URLs on microrobotica.org
  (`scripts/check_mime_docs.py live`).
* The pages are built with the `html` builder: GitHub Pages serves
  `/x/bundle.referenced-copy` from `bundle.referenced-copy.html`, dots
  and all, with no redirect.  `dirhtml` would put a 301 on every error
  link and writes `pages/errors.md` (reading an error) and
  `pages/errors/index.md` (the catalogue) to the same
  `pages/errors/index.html`, losing the first.

## Iterating on docs without committing

The default `make all-versions` builds every version from its remote
git ref.  When you're editing a docs file on the latest line and want
to see the change without committing, point the "latest" tier at your
local working copy:

```bash
make preview-local MADDENING_LOCAL=/home/you/MSF/msf/MADDENING
# Edit MADDENING/docs/... files, then re-run — <30 s incremental.
```

Older versions (v0.1, v0.2) still come from git tags — they're frozen
and don't need re-builds unless you nuke `_build/`.

## What's running

| Knob | Default | Multi-version build |
|---|---|---|
| `DOCS_MULTIVERSION` env | unset | `1` |
| `DOCS_VERSION` env | unset | the build label (`v0.1`, `v0.2`, `latest`) |
| `DOCS_DEPLOY_BASE` env | `https://microrobotica.org` | `http://localhost:8000` |
| `PROJECT` env | from caller | `maddening` / `mime` / `microrobotica` |
| `MULTIPROJECT_PATH_<PROJ>` env | unset → submodule path | tempdir worktree |
| navbar shows version switcher | no | yes (rendered, fetches JSON at page-load) |

The switcher JSON contract: a flat list of
`{version, name, url, preferred}` rows.  `version` is the value that
`DOCS_VERSION` must match for the theme to highlight the row as
"current".  `preferred: true` marks the stable release (the one new
visitors should land on if they have no preference).

## Canonical URLs and the sitemap

Multi-version docs publish the *same* page at several URLs
(`/maddening/quickstart.html`, `/maddening/v0.2/quickstart.html`,
`/maddening/v0.1/quickstart.html`).  To stop search engines treating
those as duplicate content, the **latest** version is declared
canonical:

* `conf.py` sets `html_baseurl` **without** a version segment
  (`.../maddening/`, never `.../maddening/v0.2/`).  Sphinx derives
  every page's `<link rel="canonical">` from it, so a versioned page
  emits `<link rel="canonical" href=".../maddening/<page>">` — it
  canonicalises to the *latest* copy of that page.
* The **sitemap is generated by the latest build only**.  `conf.py`
  drops the `sphinx_sitemap` extension when `DOCS_VERSION != latest`,
  so `/maddening/v0.1/` and `/maddening/v0.2/` ship no `sitemap.xml`.
  Only `/maddening/sitemap.xml` (latest) exists, and that is the one
  `seo/sitemap_index.xml` references.
* `seo/robots.txt` keeps `Allow: /` — crawlers must be able to reach
  the versioned pages so they can *see* the canonical link and
  consolidate.  Don't `Disallow` the version subdirs.

Net effect: search engines index one copy of each page (the latest),
and the version switcher is the only way readers reach older docs.

## Adding a version

`MADDENING/docs/_static/switcher.json` is the source of truth for the
MADDENING dropdown; `MICROROBOTICA/docs/_static/switcher.json` is the
(single-entry) list for the root, and MIME's list is
`MICROROBOTICA/docs/_switchers/mime/switcher.json` from 0.3 on.  A MIME
release from 0.3 on adds its `label:tag` to `MIME_RELEASES`, moves
`MIME_LATEST` to it, and adds its row to that list; a later
pre-release of the same release (0.3.0.dev2) changes only the tag.
When a project cuts a release:

1. Tag the release in that project's repo (e.g. `v0.3.0`).
2. Add a row to that project's `switcher.json` — `version` is the
   label, and move `preferred: true` to the new stable release.
3. Update the project's `*_VERSIONS` line in `docs/Makefile`, e.g.
   `MADDENING_VERSIONS := v0.1:v0.1.0 v0.2:v0.2.0 v0.3:v0.3.0 latest:origin/main`.
4. Rebuild.

The `latest` row builds from `origin/main`; the canonical
`switcher.json` it ships is whatever is on `main`, so commit the
`switcher.json` edit to `main`.

## Cleaning up

```bash
make clean              # rm -rf _build/
make clean-worktrees    # remove /tmp/msf-docs-worktree-*
```

(`WORKTREES=<dir>` puts the worktrees somewhere other than `/tmp`, for
both targets.)

`clean-worktrees` is safe to run anytime — it removes the git
worktrees the multi-version build creates under `/tmp/`.  Subsequent
builds re-create them on demand.

## Knobs that *won't* affect production

Everything in this preview is gated on env vars that default to
"production single-version":

* `make all` (no `-versions` suffix) — unchanged from v0.1 behaviour.
* CI workflow (`.github/workflows/docs.yml`) — untouched.
* The `docs/conf.py` switcher block is wrapped in
  `if _multiversion:` so it's a no-op without `DOCS_MULTIVERSION=1`.

To actually ship multi-version, the steps are in
[MADDENING's `versioned_docs.md`](https://microrobotica.org/maddening/developer_guide/versioned_docs.html)
§ "Rollout sequence" — three small CI changes once you're ready.

## Troubleshooting

**"sphinx-build not found"** — activate the docs venv
(`source ../../.venv/bin/activate`) or pass
`SPHINXBUILD=/path/to/sphinx-build`.

**"projects/maddening submodule missing"** — run
`git submodule update --init --recursive`.

**"worktree add failed for ... @ origin/..."** — the submodule's local
clone hasn't fetched that branch yet.  Run
`git -C docs/projects/maddening fetch origin --tags` and retry.

**"worktree at /tmp/... has no docs/index.md (ref too old?)"** —
the ref you're trying to build predates the current docs layout.
Drop that row from `MADDENING_VERSIONS` in the Makefile.

**Switcher shows nothing in the dropdown** — open the browser
DevTools network tab and check that `/maddening/_static/switcher.json`
returns 200.  It's served from `_build/html/maddening/_static/`; the
"latest" build is what writes it.

**Switcher shows but the current version isn't highlighted** —
the `version` field in `switcher.json` has to match `DOCS_VERSION`
exactly.  The Makefile sets `DOCS_VERSION` to the LABEL part of
the `<label>:<ref>` pair (e.g. `v0.1`, `v0.2`, `latest`).
