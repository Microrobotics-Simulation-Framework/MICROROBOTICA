# Build the unified microrobotica.org docs site.
#
# `make all` produces the full site at _build/html/, ready to drop into
# a GitHub Pages deployment:
#
#   _build/html/                  -> MICROROBOTICA (root)
#   _build/html/maddening/        -> MADDENING subproject
#   _build/html/mime/             -> MIME subproject
#
# Subprojects are built from git-submodule sources under projects/.

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     := .
BUILDDIR      := _build

# Common args: shared conf.py, parallel build, treat warnings as errors only
# in CI (set SPHINXOPTS="-W" there).
COMMON        := -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-$@ $(SPHINXOPTS)

.PHONY: help all clean microrobotica maddening mime doxygen serve sync-glossary \
        all-versions maddening-versions preview-versions clean-worktrees

help:
	@echo "Targets:"
	@echo "  all              - build all three projects (single-version) into _build/html/"
	@echo "  microrobotica    - build root project (with doxygen)"
	@echo "  maddening        - build maddening subproject under _build/html/maddening"
	@echo "  mime             - build mime subproject under _build/html/mime"
	@echo "  doxygen          - regenerate Doxygen XML (consumed by breathe)"
	@echo "  serve            - python -m http.server in _build/html/"
	@echo "  sync-glossary    - mirror docs/glossary.md into MADDENING & MIME"
	@echo "  clean            - rm -rf _build/"
	@echo ""
	@echo "Multi-version preview (v0.2 #2 follow-up — local only, not in CI):"
	@echo "  all-versions     - build EVERY project's listed versions into"
	@echo "                     _build/html/<project>/<version>/ subdirs;"
	@echo "                     'latest' lives at _build/html/<project>/."
	@echo "  maddening-versions - build only MADDENING versions"
	@echo "  preview-versions - all-versions + python http.server at :8000."
	@echo "                     Drop the trailing version in the URL to see"
	@echo "                     the switcher: http://localhost:8000/maddening/"
	@echo "  clean-worktrees  - remove temp git worktrees in /tmp"

sync-glossary:
	python3 ../scripts/sync_glossary.py

doxygen:
	@mkdir -p _doxygen/xml
	cd .. && doxygen Doxyfile

# sphinx-multiproject rewrites the srcdir based on $PROJECT — we always
# pass conf.py's directory ($(SOURCEDIR)) as both confdir and srcdir.

microrobotica: doxygen
	PROJECT=microrobotica $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html $(COMMON)

maddening:
	@test -f projects/maddening/docs/index.md \
	  || (echo "ERROR: projects/maddening submodule missing or out of date." \
	      "Run: git submodule update --init --recursive" && exit 1)
	PROJECT=maddening $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html/maddening $(COMMON)

mime:
	@test -f projects/mime/docs/index.md \
	  || (echo "ERROR: projects/mime submodule missing or out of date." \
	      "Run: git submodule update --init --recursive" && exit 1)
	PROJECT=mime $(SPHINXBUILD) -b html $(SOURCEDIR) $(BUILDDIR)/html/mime $(COMMON)

# `microrobotica` last because its html_extra_path copies assets/ into
# the root output, and we don't want subproject builds to overwrite the
# top-level CNAME / .nojekyll / videos.
all: maddening mime microrobotica
	@touch $(BUILDDIR)/html/.nojekyll
	@cp CNAME $(BUILDDIR)/html/CNAME 2>/dev/null || true

serve: all
	@cd $(BUILDDIR)/html && python3 -m http.server 8000

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
MADDENING_VERSIONS := v0.1:v0.1.0 v0.2-dev:origin/feat/v0.2 latest:origin/feat/v0.2
MIME_VERSIONS      := latest:origin/main
MICROROBOTICA_VERSIONS := latest:origin/main

# Build a single (project, version_label, git_ref) tuple via a temp
# worktree.  Usage:
#   make _build-version PROJECT=maddening LABEL=v0.1 REF=v0.1.0
#
# When LABEL=latest the output lives at _build/html/<project>/
# (unversioned URL); otherwise at _build/html/<project>/<label>/.
.PHONY: _build-version
_build-version:
	@if [ -z "$(PROJECT)" ] || [ -z "$(LABEL)" ] || [ -z "$(REF)" ]; then \
	  echo "Usage: make _build-version PROJECT=<p> LABEL=<l> REF=<r>"; exit 2; \
	fi
	@PROJ_UPPER=$$(echo $(PROJECT) | tr a-z A-Z); \
	case "$(PROJECT)" in \
	  maddening|mime) \
	    WT=/tmp/msf-docs-worktree-$(PROJECT)-$(LABEL); \
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
	  DOCS_DEPLOY_BASE=$${DOCS_DEPLOY_BASE:-http://localhost:8000} \
	  MULTIPROJECT_PATH_$$PROJ_UPPER=$$SOURCE_OVERRIDE_DIR \
	  $(SPHINXBUILD) -b html $(SOURCEDIR) $$OUT \
	    -c $(SOURCEDIR) -d $(BUILDDIR)/.doctrees-$(PROJECT)-$(LABEL) \
	    $(SPHINXOPTS)

# Expand a project's VERSIONS list into a sequence of _build-version
# calls.  Each PAIR is "label:ref".
maddening-versions:
	@test -e projects/maddening/.git \
	  || (echo "ERROR: projects/maddening submodule missing." \
	      "Run: git submodule update --init --recursive" && exit 1)
	@for pair in $(MADDENING_VERSIONS); do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
	  $(MAKE) _build-version PROJECT=maddening LABEL=$$LABEL REF=$$REF \
	    || exit $$?; \
	done

mime-versions:
	@test -e projects/mime/.git \
	  || (echo "ERROR: projects/mime submodule missing." \
	      "Run: git submodule update --init --recursive" && exit 1)
	@for pair in $(MIME_VERSIONS); do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
	  $(MAKE) _build-version PROJECT=mime LABEL=$$LABEL REF=$$REF \
	    || exit $$?; \
	done

# The root MICROROBOTICA project only has one version today; skip the
# version subdir loop and just build it normally with DOCS_MULTIVERSION=1.
# Doxygen is optional for the local preview (the C++ API pages won't
# resolve, but the version switcher + cross-project navigation work).
microrobotica-versions:
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
	@echo "Multi-version site built.  Preview with:"
	@echo "    cd $(BUILDDIR)/html && python3 -m http.server 8000"
	@echo "or simply: make preview-versions"

preview-versions: all-versions
	@cd $(BUILDDIR)/html && python3 -m http.server 8000

# Fast-iteration variant: build the "latest" tier from a local working
# copy instead of fetching origin.  Useful when you're editing
# docs/_static/switcher.json or any other v0.2 doc and want to see the
# change in <30 s without committing.  Other versions still come from
# git refs.
#
# Usage:
#   make preview-local MADDENING_LOCAL=/home/me/MSF/msf/MADDENING
preview-local:
	@if [ -z "$(MADDENING_LOCAL)" ]; then \
	  echo "Usage: make preview-local MADDENING_LOCAL=/path/to/MADDENING"; \
	  exit 2; \
	fi
	@if [ ! -f "$(MADDENING_LOCAL)/docs/index.md" ]; then \
	  echo "ERROR: $(MADDENING_LOCAL)/docs/index.md not found"; exit 1; \
	fi
	@# Build the older versions normally (from refs), then build
	@# "latest" from the local working copy, overriding the worktree.
	@for pair in v0.1:v0.1.0 v0.2-dev:origin/feat/v0.2; do \
	  LABEL=$${pair%%:*}; REF=$${pair##*:}; \
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
	@echo "    cd $(BUILDDIR)/html && python3 -m http.server 8000"

clean-worktrees:
	@for wt in /tmp/msf-docs-worktree-*; do \
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
