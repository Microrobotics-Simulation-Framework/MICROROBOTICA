# Build the unified microrobotica.org docs site.
#
# `make all` produces the full site at _build/html/, ready to drop into
# a GitHub Pages deployment:
#
#   _build/html/                  -> MICROROBOTICA (root)
#   _build/html/maddening/        -> MADDENING subproject
#   _build/html/mime/             -> MIME subproject
#
# Subprojects are built from git-submodule sources under projects/, except
# MIME from 0.3 on, whose docs are generated: its pages are fetched from its
# GitHub release (MIME_RELEASES, below) at build time, never committed.

SPHINXOPTS    ?=
# Prefer the sibling shared venv (the msf/.venv layout) so `make` works
# without activating it first; fall back to whatever `sphinx-build` is on
# PATH.  Override explicitly with `make <target> SPHINXBUILD=/path/...`.
SPHINXBUILD   ?= $(firstword $(wildcard ../../.venv/bin/sphinx-build) sphinx-build)
SOURCEDIR     := .
BUILDDIR      := _build
PYTHON        ?= python3

# Common args: shared conf.py, parallel build, treat warnings as errors only
# in CI (set SPHINXOPTS="-W" there).
COMMON        := -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-$@ $(SPHINXOPTS)

.PHONY: help all clean microrobotica maddening mime doxygen serve sync-glossary \
        all-versions maddening-versions mime-versions mime-ref-versions \
        mime-release-versions mime-pages check-mime-docs microrobotica-versions \
        preview-versions preview-local clean-worktrees _check-sphinx

help:
	@echo "Targets:"
	@echo "  all              - build all three projects (single-version) into _build/html/"
	@echo "  microrobotica    - build root project (with doxygen)"
	@echo "  maddening        - build maddening subproject under _build/html/maddening"
	@echo "  mime             - build mime subproject under _build/html/mime, from"
	@echo "                     the pages of MIME's newest release (mime-pages)"
	@echo "  mime-pages       - fetch those pages (the release asset"
	@echo "                     mime-docs-<version>.tar.gz), or take MIME_PAGES_DIR"
	@echo "  doxygen          - regenerate Doxygen XML (consumed by breathe)"
	@echo "  serve            - serve _build/html/ with GitHub Pages' URL rules"
	@echo "                     (../scripts/pages_server.py) at :8000"
	@echo "  sync-glossary    - mirror docs/glossary.md into MADDENING & MIME"
	@echo "  clean            - rm -rf _build/"
	@echo ""
	@echo "Multi-version preview (v0.2 #2 follow-up — local only, not in CI):"
	@echo "  all-versions     - build EVERY project's listed versions into"
	@echo "                     _build/html/<project>/<version>/ subdirs;"
	@echo "                     'latest' lives at _build/html/<project>/."
	@echo "  maddening-versions - build only MADDENING versions"
	@echo "  mime-versions    - build only MIME versions: the git refs (v0.1, v0.2)"
	@echo "                     and the release pages (0.3.0, and /mime/ itself)"
	@echo "  check-mime-docs  - after mime-versions: every MIME error code has its"
	@echo "                     page, its extensionless URL resolves, links land"
	@echo "  preview-versions - all-versions + the Pages-rules server at :8000."
	@echo "                     Drop the trailing version in the URL to see"
	@echo "                     the switcher: http://localhost:8000/maddening/"
	@echo "  clean-worktrees  - remove temp git worktrees in /tmp"
	@echo ""
	@echo "MIME 0.3 on is built from its release's pages.  Fallback, with no"
	@echo "release asset: in a MIME checkout with its 'verify' extra,"
	@echo "    uv run --no-sync mime docs build --out <dir>"
	@echo "then:  make all-versions MIME_PAGES_DIR=<dir>"

sync-glossary:
	python3 ../scripts/sync_glossary.py

# Fail early with an actionable message if Sphinx isn't reachable,
# instead of the cryptic `env: 'sphinx-build': No such file or directory`.
_check-sphinx:
	@command -v $(SPHINXBUILD) >/dev/null 2>&1 || { \
	  echo "ERROR: sphinx-build not found (tried: $(SPHINXBUILD))."; \
	  echo "  Activate the docs venv:   source ../../.venv/bin/activate"; \
	  echo "  and install docs deps:    pip install -r requirements.txt"; \
	  echo "  or pass one explicitly:   make <target> SPHINXBUILD=/path/to/sphinx-build"; \
	  exit 127; }

doxygen:
	@mkdir -p _doxygen/xml
	cd .. && doxygen Doxyfile

# sphinx-multiproject rewrites the srcdir based on $PROJECT — we always
# pass conf.py's directory ($(SOURCEDIR)) as both confdir and srcdir.

microrobotica: _check-sphinx doxygen
	PROJECT=microrobotica $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html $(COMMON)

maddening: _check-sphinx
	@test -f projects/maddening/docs/index.md \
	  || (echo "ERROR: projects/maddening submodule missing or out of date." \
	      "Run: git submodule update --init --recursive" && exit 1)
	PROJECT=maddening $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html/maddening $(COMMON)

# MIME 0.3 on: MIME's docs are the pages of its newest release (see
# MIME_RELEASES below), not the submodule's docs/.
mime: _check-sphinx mime-pages
	PROJECT=mime MULTIPROJECT_PATH_MIME=$(abspath $(MIME_PAGES_ROOT)/$(MIME_LATEST)) \
	  $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html/mime $(COMMON)

# `microrobotica` last because its html_extra_path copies assets/ into
# the root output, and we don't want subproject builds to overwrite the
# top-level CNAME / .nojekyll / videos.
all: maddening mime microrobotica
	@touch $(BUILDDIR)/html/.nojekyll
	@cp CNAME $(BUILDDIR)/html/CNAME 2>/dev/null || true

# GitHub Pages serves /x from x.html; `python3 -m http.server` does not, so
# the extensionless links MIME's errors print only work with this server.
serve: all
	@$(PYTHON) ../scripts/pages_server.py $(BUILDDIR)/html --port 8000

clean:
	rm -rf $(BUILDDIR)


# -- Multi-version preview ------------------------------------------------
# v0.2 #2 follow-up: build every listed version of every project into
# /<project>/<version>/ and the "latest" build into /<project>/.  All
# versions reference the same root-relative switcher.json at
# /<project>/_static/switcher.json (written by the "latest" build),
# so the dropdown self-updates as new versions are added.
#
# Each version is built from a temporary git worktree under /tmp so
# the user's submodule checkout isn't mutated.  Worktrees are
# auto-cleaned by `clean-worktrees`.

# Project → space-separated list of (label : git_ref) pairs.  The LAST
# pair in each list is the one that builds to the *unversioned* URL
# (the "latest" alias).
#
# Convention for the label:
#   - tag names without the leading "v" (e.g. "0.1" for v0.1.0)
#     when the ref is a release tag
#   - "<minor>-dev" when the ref is an in-flight feature branch
#
# Add a row when cutting a release: tag, then add the tag here, then
# rebuild.  The switcher.json file in MADDENING/docs/_static/ stays
# in sync (manually edited at release time).
# Use `origin/<branch>` for branch refs so a fresh `git fetch` picks
# up the latest tip without requiring a local tracking branch.  Tags
# don't need the `origin/` prefix.
MADDENING_VERSIONS := v0.1:v0.1.0 v0.2:v0.2.0 v0.2.1:v0.2.1 v0.3.0:v0.3.0 v0.4.0:v0.4.0 v0.4.1:v0.4.1 v0.4.2:v0.4.2 latest:origin/main
MIME_VERSIONS      := v0.1:v0.1.0 v0.2:v0.2.0
# MIME 0.3 on, the docs are generated: `mime docs build` writes index.md and
# pages/ from a checkout and its installed registry, and a git ref's docs/
# holds no index.md.  MIME's release attaches the pages its own CI built
# (with the evidence counts read) as mime-docs-<version>.tar.gz; each
# (label : tag) here is built from that asset into _build/html/mime/<label>/.
# The label is the release number with no "v": MIME's errors link to
# https://microrobotica.org/mime/<release>/pages/errors/<code> (0.3.0 for
# 0.3.0.dev1).  A later 0.3.0.devN or 0.3.0 changes the tag, not the label.
MIME_RELEASES      := 0.3.0:v0.3.0.dev1
# The release also built to the unversioned /mime/ ("latest"): MIME's main
# is the 0.3 line from its release on, with no docs/index.md.  The switcher
# marks /mime/ as this label.
MIME_LATEST        := 0.3.0
# Fallback: pages already built by `mime docs build --out <dir>` in a MIME
# checkout, used for $(MIME_LATEST) in place of its release asset (before
# the release, or to read a change).  Their evidence counts read "not read".
MIME_PAGES_DIR     ?=
MIME_PAGES_ROOT    := $(BUILDDIR)/mime-pages
MICROROBOTICA_VERSIONS := latest:origin/main

# Build a single (project, version_label, git_ref) tuple via a temp
# worktree.  Usage:
#   make _build-version PROJECT=maddening LABEL=v0.1 REF=v0.1.0
#
# When LABEL=latest the output lives at _build/html/<project>/
# (unversioned URL); otherwise at _build/html/<project>/<label>/.
# SRC=<dir> builds from a directory of sources (a MIME release's pages) in
# place of a worktree at REF; MATCH=<label> is the switcher row the build
# marks as its own (default: LABEL).  WORKTREES is where the temporary
# worktrees go.
WORKTREES ?= /tmp
.PHONY: _build-version
_build-version:
	@if [ -z "$(PROJECT)" ] || [ -z "$(LABEL)" ] || [ -z "$(REF)" ]; then \
	  echo "Usage: make _build-version PROJECT=<p> LABEL=<l> REF=<r> [SRC=<dir>] [MATCH=<l>]"; exit 2; \
	fi
	@PROJ_UPPER=$$(echo $(PROJECT) | tr a-z A-Z); \
	case "$(PROJECT)" in \
	  maddening|mime) \
	    if [ -n "$(SRC)" ]; then \
	      test -f $(SRC)/index.md || { echo "$(SRC) has no index.md"; exit 1; }; \
	      SOURCE_OVERRIDE_DIR=$(abspath $(SRC)); \
	    else \
	    WT=$(WORKTREES)/msf-docs-worktree-$(PROJECT)-$(LABEL); \
	    SUBMOD=projects/$(PROJECT); \
	    if [ ! -e $$SUBMOD/.git ]; then \
	      echo "ERROR: $$SUBMOD submodule not initialised."; \
	      echo "Run: git submodule update --init --recursive"; \
	      exit 1; \
	    fi; \
	    git -C $$SUBMOD fetch origin --quiet --tags || true; \
	    git -C $$SUBMOD worktree prune; \
	    if [ ! -d $$WT ]; then \
	      git -C $$SUBMOD worktree add --detach $$WT $(REF) \
	        || { echo "worktree add failed for $$WT @ $(REF)"; exit 1; }; \
	    else \
	      (cd $$WT && git fetch origin --quiet --tags || true; \
	         git checkout --detach $(REF)) \
	        || { echo "worktree update failed for $$WT @ $(REF)"; exit 1; }; \
	    fi; \
	    test -f $$WT/docs/index.md \
	      || { echo "worktree at $$WT has no docs/index.md (ref $(REF) too old?)"; exit 1; }; \
	    SOURCE_OVERRIDE_DIR=$$WT/docs; \
	    fi; \
	    ;; \
	  microrobotica) \
	    SOURCE_OVERRIDE_DIR=.; \
	    ;; \
	esac; \
	if [ "$(LABEL)" = "latest" ]; then \
	  OUT=$(BUILDDIR)/html/$(PROJECT); \
	else \
	  OUT=$(BUILDDIR)/html/$(PROJECT)/$(LABEL); \
	fi; \
	echo ">> $(PROJECT) $(LABEL) ($(REF))  →  $$OUT"; \
	env PROJECT=$(PROJECT) \
	  DOCS_MULTIVERSION=1 DOCS_VERSION=$(LABEL) \
	  DOCS_SWITCHER_MATCH=$(or $(MATCH),$(LABEL)) \
	  DOCS_DEPLOY_BASE=$${DOCS_DEPLOY_BASE:-http://localhost:8000} \
	  MULTIPROJECT_PATH_$$PROJ_UPPER=$$SOURCE_OVERRIDE_DIR \
	  $(SPHINXBUILD) -b html $(SOURCEDIR) $$OUT \
	    -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-$(PROJECT)-$(LABEL) \
	    $(SPHINXOPTS)

# Expand a project's VERSIONS list into a sequence of _build-version
# calls.  Each PAIR is "label:ref".
maddening-versions: _check-sphinx
	@test -e projects/maddening/.git \
	  || (echo "ERROR: projects/maddening submodule missing." \
	      "Run: git submodule update --init --recursive" && exit 1)
	@for pair in $(MADDENING_VERSIONS); do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
	  $(MAKE) _build-version PROJECT=maddening LABEL=$$LABEL REF=$$REF \
	    || exit $$?; \
	done

mime-versions: mime-ref-versions mime-release-versions

mime-ref-versions: _check-sphinx
	@test -e projects/mime/.git \
	  || (echo "ERROR: projects/mime submodule missing." \
	      "Run: git submodule update --init --recursive" && exit 1)
	@for pair in $(MIME_VERSIONS); do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
	  $(MAKE) _build-version PROJECT=mime LABEL=$$LABEL REF=$$REF \
	    || exit $$?; \
	done

# Fetch each release's pages into $(MIME_PAGES_ROOT)/<label>/ (under
# _build/, ignored by git, never committed): the release asset, or
# MIME_PAGES_DIR for $(MIME_LATEST).
mime-pages:
	@for pair in $(MIME_RELEASES); do \
	  LABEL=$${pair%%:*}; TAG=$${pair##*:}; \
	  if [ -n "$(MIME_PAGES_DIR)" ] && [ "$$LABEL" = "$(MIME_LATEST)" ]; then \
	    echo ">> mime $$LABEL: pages from MIME_PAGES_DIR (the fallback, not the release asset)"; \
	    $(PYTHON) ../scripts/fetch_mime_docs.py --from "$(MIME_PAGES_DIR)" \
	      --out $(MIME_PAGES_ROOT)/$$LABEL || exit $$?; \
	  else \
	    $(PYTHON) ../scripts/fetch_mime_docs.py --tag $$TAG \
	      --out $(MIME_PAGES_ROOT)/$$LABEL || exit $$?; \
	  fi; \
	done

# Build each release's pages into /mime/<label>/, then the newest again into
# /mime/ (where the canonical URLs, the sitemap and the switcher.json every
# version reads live).
mime-release-versions: _check-sphinx mime-pages
	@for pair in $(MIME_RELEASES); do \
	  LABEL=$${pair%%:*}; TAG=$${pair##*:}; \
	  $(MAKE) _build-version PROJECT=mime LABEL=$$LABEL REF=$$TAG \
	    SRC=$(MIME_PAGES_ROOT)/$$LABEL || exit $$?; \
	done
	@$(MAKE) _build-version PROJECT=mime LABEL=latest REF=$(MIME_LATEST) \
	  SRC=$(MIME_PAGES_ROOT)/$(MIME_LATEST) MATCH=$(MIME_LATEST)

# After mime-versions: every code MIME's catalogue lists has its page under
# /mime/$(MIME_LATEST)/ and /mime/, each extensionless .../pages/errors/<code>
# answers with it under GitHub Pages' rules, and every link of the release's
# pages lands.  Writes the codes for the post-deploy sample.
# MIME_REFUSAL_TABLE=<checkout>/src/mime/build/refusal_table.txt compares
# the catalogue with a checkout's table too.
check-mime-docs:
	$(PYTHON) ../scripts/check_mime_docs.py build \
	  --pages $(MIME_PAGES_ROOT)/$(MIME_LATEST) --site $(BUILDDIR)/html \
	  --version $(MIME_LATEST) --manifest $(BUILDDIR)/mime-error-codes.json \
	  $(if $(MIME_REFUSAL_TABLE),--refusal-table $(MIME_REFUSAL_TABLE))

# The root MICROROBOTICA project only has one version today; skip the
# version subdir loop and just build it normally with DOCS_MULTIVERSION=1.
# Doxygen is optional for the local preview (the C++ API pages won't
# resolve, but the version switcher + cross-project navigation work).
microrobotica-versions: _check-sphinx
	@if command -v doxygen >/dev/null 2>&1; then \
	  $(MAKE) doxygen; \
	else \
	  echo "doxygen not installed — skipping (C++ API pages will be empty)"; \
	  mkdir -p _doxygen/xml; \
	fi
	DOCS_MULTIVERSION=1 DOCS_VERSION=latest \
	  DOCS_DEPLOY_BASE=$${DOCS_DEPLOY_BASE:-http://localhost:8000} \
	  PROJECT=microrobotica \
	  $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html \
	    -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-microrobotica-versions \
	    $(SPHINXOPTS)

all-versions: maddening-versions mime-versions microrobotica-versions
	@touch $(BUILDDIR)/html/.nojekyll
	@cp CNAME $(BUILDDIR)/html/CNAME 2>/dev/null || true
	@echo ""
	@echo "Multi-version site built.  Check MIME's error pages:  make check-mime-docs"
	@echo "Preview with GitHub Pages' URL rules:"
	@echo "    $(PYTHON) ../scripts/pages_server.py $(BUILDDIR)/html --port 8000"
	@echo "or simply: make preview-versions"

preview-versions: all-versions
	@$(PYTHON) ../scripts/pages_server.py $(BUILDDIR)/html --port 8000

# Fast-iteration variant: build the "latest" tier from a local working
# copy instead of fetching origin.  Useful when you're editing
# docs/_static/switcher.json or any other v0.2 doc and want to see the
# change in <30 s without committing.  Other versions still come from
# git refs.
#
# Usage:
#   make preview-local MADDENING_LOCAL=/home/me/MSF/msf/MADDENING
preview-local: _check-sphinx
	@if [ -z "$(MADDENING_LOCAL)" ]; then \
	  echo "Usage: make preview-local MADDENING_LOCAL=/path/to/MADDENING"; \
	  exit 2; \
	fi
	@if [ ! -f "$(MADDENING_LOCAL)/docs/index.md" ]; then \
	  echo "ERROR: $(MADDENING_LOCAL)/docs/index.md not found"; exit 1; \
	fi
	@# Build every non-"latest" version from its git ref, then build
	@# "latest" from the local working copy.  Derived from the single
	@# MADDENING_VERSIONS list so the version set never drifts.
	@for pair in $(MADDENING_VERSIONS); do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
	  [ "$$LABEL" = latest ] && continue; \
	  $(MAKE) _build-version PROJECT=maddening LABEL=$$LABEL REF=$$REF \
	    || exit $$?; \
	done
	@echo ">> maddening latest from local working copy $(MADDENING_LOCAL)"
	@env PROJECT=maddening DOCS_MULTIVERSION=1 DOCS_VERSION=latest \
	  DOCS_DEPLOY_BASE=$${DOCS_DEPLOY_BASE:-http://localhost:8000} \
	  MULTIPROJECT_PATH_MADDENING=$(MADDENING_LOCAL)/docs \
	  $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html/maddening \
	    -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-maddening-local \
	    $(SPHINXOPTS)
	@$(MAKE) mime-versions microrobotica-versions
	@touch $(BUILDDIR)/html/.nojekyll
	@echo ""
	@echo "Local-preview site built.  Serve with:"
	@echo "    $(PYTHON) ../scripts/pages_server.py $(BUILDDIR)/html --port 8000"

clean-worktrees:
	@for wt in $(WORKTREES)/msf-docs-worktree-*; do \
	  [ -d $$wt ] || continue; \
	  proj=$$(basename $$wt | cut -d- -f4); \
	  echo "Removing worktree $$wt"; \
	  if [ -d projects/$$proj/.git ]; then \
	    git -C projects/$$proj worktree remove --force $$wt 2>/dev/null \
	      || rm -rf $$wt; \
	  else \
	    rm -rf $$wt; \
	  fi; \
	done
