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

.PHONY: help all clean microrobotica maddening mime doxygen serve

help:
	@echo "Targets:"
	@echo "  all           - build all three projects into _build/html/"
	@echo "  microrobotica - build root project (with doxygen)"
	@echo "  maddening     - build maddening subproject under _build/html/maddening"
	@echo "  mime          - build mime subproject under _build/html/mime"
	@echo "  doxygen       - regenerate Doxygen XML (consumed by breathe)"
	@echo "  serve         - python -m http.server in _build/html/"
	@echo "  clean         - rm -rf _build/"

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
