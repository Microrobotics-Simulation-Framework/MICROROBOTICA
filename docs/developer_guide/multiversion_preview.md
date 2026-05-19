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
make preview-versions     # builds, then runs http.server on :8000

# Open http://localhost:8000/maddening/ in a browser.
# The version switcher is in the top navbar.
```

The output structure mirrors the production target:

```
_build/html/
├── index.html                              ← MICROROBOTICA latest
├── maddening/
│   ├── index.html                          ← MADDENING latest (= feat/v0.2 tip)
│   ├── _static/switcher.json               ← canonical version list
│   ├── v0.1/index.html                     ← MADDENING v0.1.0 tag
│   └── v0.2-dev/index.html                 ← MADDENING feat/v0.2 tip (preview)
└── mime/
    └── index.html                          ← MIME latest
```

Every version's HTML references `/maddening/_static/switcher.json` via
a root-relative URL, so v0.1 pages pick up the switcher dropdown even
though v0.1's source tree predates the switcher.

## Iterating on docs without committing

The default `make all-versions` builds every version from its remote
git ref.  When you're editing a v0.2 docs file and want to see the
change without committing, point the "latest" tier at your local
working copy:

```bash
make preview-local MADDENING_LOCAL=/home/you/MSF/msf/MADDENING
# Edit MADDENING/docs/... files, then:
make preview-local MADDENING_LOCAL=/home/you/MSF/msf/MADDENING
# Re-runs in <30 s for incremental changes.
```

Older versions (v0.1) still come from git tags — they're frozen and
don't need re-builds unless you nuke `_build/`.

## What's running

| Knob | Default | Multi-version build |
|---|---|---|
| `DOCS_MULTIVERSION` env | unset | `1` |
| `DOCS_VERSION` env | unset | the build label (`v0.1`, `v0.2-dev`, `latest`) |
| `DOCS_DEPLOY_BASE` env | `https://microrobotica.org` | `http://localhost:8000` |
| `PROJECT` env | from caller | `maddening` / `mime` / `microrobotica` |
| `MULTIPROJECT_PATH_<PROJ>` env | unset → submodule path | tempdir worktree |
| navbar shows version switcher | no | yes (rendered, fetches JSON at page-load) |

The switcher JSON contract: a flat list of
`{version, name, url, preferred}` rows.  `version` is the value that
`DOCS_VERSION` must match for the theme to highlight the row as
"current".  `preferred: true` marks the stable release (the one
new visitors should land on if they have no preference).

## Editing the version list

`MADDENING/docs/_static/switcher.json` is the source of truth for
which MADDENING versions appear in the dropdown.  Edit it directly
and re-run `make preview-local` to see the new list.

When v0.2 ships as a tag:

1. Rename `v0.2-dev` → `v0.2` in `switcher.json` and add the
   release URL.
2. Tag `v0.2.0` in MADDENING.
3. Update `MADDENING_VERSIONS` in `docs/Makefile` to read
   `v0.2:v0.2.0` instead of `v0.2-dev:origin/feat/v0.2`.
4. Push.

## Cleaning up

```bash
make clean              # rm -rf _build/
make clean-worktrees    # remove /tmp/msf-docs-worktree-*
```

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

**"projects/maddening submodule missing"** — run
`git submodule update --init --recursive`.

**"worktree add failed for ... @ origin/..."** — the submodule's local
clone hasn't fetched that branch yet.  Run
`git -C docs/projects/maddening fetch origin --tags` and retry.

**"worktree at /tmp/... has no docs/index.md (ref too old?)"** —
the ref you're trying to build predates the current docs layout
(e.g. v0.0 from before `docs/` existed).  Drop that row from
`MADDENING_VERSIONS` in the Makefile.

**Switcher shows nothing in the dropdown** — open the browser
DevTools network tab and check that `/maddening/_static/switcher.json`
returns 200.  It's served from `_build/html/maddening/_static/`; the
"latest" build is what writes it.

**Switcher shows but the current version isn't highlighted** —
the `version` field in `switcher.json` has to match `DOCS_VERSION`
exactly.  The Makefile sets `DOCS_VERSION` to the LABEL part of
the `<label>:<ref>` pair (e.g. `v0.1`, `v0.2-dev`, `latest`).
