"""Shared Sphinx configuration for the unified microrobotica.org docs site.

The site is built with `sphinx-multiproject`: this single conf.py serves
three Sphinx projects (microrobotica, maddening, mime) selected by the
PROJECT environment variable. Each project's source tree lives in:

    microrobotica   .                          (this directory)
    maddening       projects/maddening/docs    (git submodule -> MADDENING)
    mime            projects/mime/docs         (git submodule -> MIME)

Build the full site with `make all` from this directory.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

# Local Sphinx extensions live under docs/_ext/
sys.path.insert(0, str(Path(__file__).parent / "_ext"))

# ── multiproject ─────────────────────────────────────────────────────
from multiproject.utils import get_project

multiproject_projects = {
    "microrobotica": {"path": "."},
    "maddening":     {"path": "projects/maddening/docs"},
    "mime":          {"path": "projects/mime/docs"},
}

# Allow the multi-version preview Makefile target to point a project at
# a git worktree of a specific tag/branch rather than the live
# submodule (which is one commit on one ref).  The override env var
# convention is MULTIPROJECT_PATH_<UPPER_PROJECT>=<dir>.
for _p in multiproject_projects:
    _override = os.environ.get(f"MULTIPROJECT_PATH_{_p.upper()}")
    if _override:
        multiproject_projects[_p]["path"] = _override

current_project = get_project(multiproject_projects)

# ── per-project metadata ─────────────────────────────────────────────
_GH_ORG = "https://github.com/Microrobotics-Simulation-Framework"

_PROJECT_META = {
    "microrobotica": {
        "project": "MICROROBOTICA",
        "tagline": "MICROROBOTics Iterative simulation for Clinical Adoption",
        "repo":    f"{_GH_ORG}/MICROROBOTICA",
    },
    "maddening": {
        "project": "MADDENING",
        "tagline": "Modular Automatic Differentiation and Data Enhanced "
                   "Neural-network INteracting Graph",
        "repo":    f"{_GH_ORG}/MADDENING",
    },
    "mime": {
        "project": "MIME",
        "tagline": "MIcrorobotics Multiphysics Engine",
        "repo":    f"{_GH_ORG}/MIME",
    },
}

project = _PROJECT_META[current_project]["project"]
_tagline = _PROJECT_META[current_project]["tagline"]
_repo_url = _PROJECT_META[current_project]["repo"]
copyright = "2026, Nicholas Roy"
author = "Nicholas Roy"
release = "0.1.0"

# ── extensions ───────────────────────────────────────────────────────
extensions = [
    "multiproject",
    "myst_parser",
    "sphinx_design",
    "sphinx.ext.intersphinx",
    "sphinx.ext.autosectionlabel",
    "sphinx.ext.mathjax",
    "sphinxcontrib.mermaid",
    "sphinx_copybutton",
    "sphinx_sitemap",
    "sphinx_tippy",
    "mermaid_tips",
]

# ── sphinx-tippy ─────────────────────────────────────────────────────
# Pop up a tippy.js bubble whenever the cursor lands on an internal
# cross-reference (glossary `{term}`s, autosectionlabel refs, footnotes).
tippy_props = {
    "placement": "auto-end",
    "maxWidth": 360,
    "theme": "material",
    "interactive": True,
}
# pydata-sphinx-theme wraps page content in <article class="bd-article">.
tippy_anchor_parent_selector = "article.bd-article"
# Skip the silent ¶ headerlinks that Sphinx puts on every heading; otherwise
# every header gets an unhelpful "permalink to this heading" bubble.
tippy_skip_anchor_classes = ("headerlink",)
# We rely on sphinx-tippy's built-in cross-reference detection only —
# no live network fetches at build time.
tippy_enable_mathjax = False
tippy_enable_doitips = False
tippy_enable_wikitips = False
tippy_rtd_urls = []
# Only decorate `{term}` references with tooltips. Sphinx-tippy by default
# attaches to *every* internal cross-reference (doc links, autosection
# anchors, etc.), which is too noisy. The regex below skips any href that
# does not contain `#term-…` (negative lookahead).
tippy_skip_urls = (r"^(?!.*#term-).*$",)
# Pin tippy + popper to specific versions so unpkg skips its `latest`
# redirect. We use the *bundle* build of tippy (auto-injects tippy's
# base CSS at runtime — without it the bubble has no background); both
# the bundle and the plain UMD still require a separate `window.Popper`
# loaded before them, hence popper is its own entry.
tippy_js = (
    "https://unpkg.com/@popperjs/core@2.11.8/dist/umd/popper.min.js",
    "https://unpkg.com/tippy.js@6.3.7/dist/tippy-bundle.umd.min.js",
)

# Breathe (C++ via Doxygen XML) only for MICROROBOTICA.
if current_project == "microrobotica":
    extensions.append("breathe")
    breathe_projects = {"MICROROBOTICA": "_doxygen/xml"}
    breathe_default_project = "MICROROBOTICA"

# MyST extensions.
myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "dollarmath",
    "html_image",
    "linkify",
    "substitution",
]
myst_heading_anchors = 3
myst_dmath_double_inline = True

# sphinx-copybutton: don't copy shell prompts
copybutton_prompt_text = r"\$ |>>> |\.\.\. "
copybutton_prompt_is_regexp = True

# sphinxcontrib-mermaid: pin a specific minor so the CDN doesn't have to
# redirect through `@11`.
mermaid_version = "11.15.0"
# Disable the D3-zoom integration: it pulls d3.min.js (~91 KB gz / 273 KB
# raw) on every page, blocking interaction-readiness. Our own pure-pointer
# pan+zoom in _static/msf-diagrams.js handles the hero-modal diagrams;
# inline diagrams stay static (a fine tradeoff for the byte savings).
mermaid_d3_zoom = False
mermaid_zoom_max_items = 0

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

templates_path = ["_templates"]
exclude_patterns = [
    "_build",
    "_doxygen",
    "Thumbs.db",
    ".DS_Store",
    "projects",  # subprojects build with their own srcdir; exclude from microrobotica root
    "tmp",
    "**/.venv",
]

# autosectionlabel — only generate labels per-document to avoid collisions
# between the three projects (which all have e.g. "Installation" headings).
autosectionlabel_prefix_document = True
autosectionlabel_maxdepth = 2

# ── HTML theme ───────────────────────────────────────────────────────
html_theme = "pydata_sphinx_theme"
html_static_path = ["_static"]
# Also pick up the active sub-project's own _static/ (in particular
# its switcher.json) when building a non-root project.  Resolved as
# an absolute path so it works regardless of CWD or worktree path.
_subproject_static = os.path.abspath(
    os.path.join(multiproject_projects[current_project]["path"], "_static"),
)
if current_project != "microrobotica" and os.path.isdir(_subproject_static):
    html_static_path.append(_subproject_static)
html_css_files = ["custom.css"]
# msf-mermaid-tippy.js promotes mermaid <title> tags and the sidecar
# `mermaid-tips` directive into tippy.js bubbles — needed on every project.
html_js_files = ["msf-mermaid-tippy.js"]
# msf-diagrams.js defines window.MSF_DIAGRAMS used by the landing page modal.
# Only loaded for the microrobotica build, which is the only page that uses it.
if current_project == "microrobotica":
    html_js_files = ["msf-diagrams.js", "msf-mermaid-tippy.js"]

# Copy `assets/` (videos and other large media kept out of _static/) to
# the build output as-is. Only on the microrobotica project — the
# subprojects do not currently have assets to ship.
#
# `seo/` holds the cross-project sitemap_index.xml + robots.txt that
# tie the three per-project sitemaps together at the site root. Only
# the microrobotica build writes to the root, so we only ship them
# from this project.
if current_project == "microrobotica":
    html_extra_path = ["assets", "seo"]

# sphinx-sitemap: each subproject emits its own sitemap.xml relative
# to its public URL. The cross-project sitemap_index.xml in seo/
# stitches them together at https://microrobotica.org/sitemap_index.xml.
_BASEURLS = {
    "microrobotica": "https://microrobotica.org/",
    "maddening":     "https://microrobotica.org/maddening/",
    "mime":          "https://microrobotica.org/mime/",
}
html_baseurl = _BASEURLS[current_project]
sitemap_url_scheme = "{link}"
html_logo = None  # text logo from theme_options below
html_favicon = None

# When deployed on GitHub Pages, each subproject lives at /<name>/.
# We tell pydata-sphinx-theme where each one lives so its sidebar and
# breadcrumbs render correct cross-project links.
_DEPLOY_BASE = os.environ.get("DOCS_DEPLOY_BASE", "https://microrobotica.org")

# v0.2 preview: per-version site is opt-in via env vars so this branch
# can be merged ahead of the actual release without affecting prod.
#
#   DOCS_MULTIVERSION=1    enables the PyData version-switcher dropdown
#                          and adds it to the navbar.  Off by default
#                          (so the production CI build keeps producing
#                          single-version output).
#   DOCS_VERSION=<tag>     tells the theme which switcher.json row is
#                          the current page ("latest" by default).
#   DOCS_DEPLOY_BASE=<url> the public root URL.  Overridden to
#                          "http://localhost:8000" for local preview;
#                          falls back to https://microrobotica.org for
#                          published builds.
_multiversion = os.environ.get("DOCS_MULTIVERSION") == "1"
_docs_version = os.environ.get("DOCS_VERSION", "latest")

html_theme_options = {
    "logo": {
        # Short brand name; always links back to the root site regardless of
        # which sub-project is being built.
        "text": "MSF",
        "link": "https://microrobotica.org/",
    },
    "navbar_start": ["navbar-logo"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": (
        ["theme-switcher", "version-switcher", "navbar-icon-links"]
        if _multiversion
        else ["theme-switcher", "navbar-icon-links"]
    ),
    "navbar_persistent": ["search-button"],
    "navbar_align": "left",
    "show_nav_level": 2,
    "navigation_depth": 4,
    "show_toc_level": 2,
    # Keep the top navbar items visible before collapsing to "More".
    # MIME has many toctree captions so we keep this small.
    "header_links_before_dropdown": 3,
    "use_edit_page_button": False,
    "external_links": [
        {"name": "MADDENING", "url": f"{_DEPLOY_BASE}/maddening/"},
        {"name": "MIME",      "url": f"{_DEPLOY_BASE}/mime/"},
        {"name": "MICROROBOTICA", "url": f"{_DEPLOY_BASE}/"},
    ],
    "icon_links": [
        {
            "name": f"{project} on GitHub",
            "url": _repo_url,
            "icon": "fa-brands fa-github",
            "type": "fontawesome",
        },
        {
            "name": "All MSF repos",
            "url": _GH_ORG,
            "icon": "fa-solid fa-cubes",
            "type": "fontawesome",
        },
    ],
    "announcement": (
        f"<p><strong>{project}</strong> &middot; {_tagline}. "
        f"<a href=\"{_repo_url}\" class=\"reference external\">Source on GitHub \u2192</a> "
        "&middot; <em>Docs are scaffolding \u2014 pages will fill in over the coming weeks.</em></p>"
    ),
    "footer_start": ["copyright"],
    "footer_end": ["theme-version"],
}

if _multiversion:
    # Root-relative URL so it works on both
    # https://microrobotica.org/<project>/<version>/ and on a local
    # preview at http://localhost:8000/<project>/<version>/.  All
    # versions fetch the SAME switcher.json (the one at the
    # unversioned URL) — that way an old release picks up newer
    # entries automatically when the canonical JSON is updated.
    _project_root = f"/{current_project}/" if current_project != "microrobotica" else "/"
    html_theme_options["switcher"] = {
        "json_url": f"{_project_root}_static/switcher.json",
        "version_match": _docs_version,
    }
    # When set, also include `check_switcher: False` to silence
    # the theme's startup warning if the JSON URL isn't reachable
    # at build time (it won't be, until make all-versions has run).
    html_theme_options["check_switcher"] = False

html_context = {
    "current_project": current_project,
    "deploy_base": _DEPLOY_BASE,
    "multiversion": _multiversion,
    "docs_version": _docs_version,
}

html_title = f"{project} — {_tagline}"
html_short_title = project

# ── intersphinx (cross-project references) ───────────────────────────
intersphinx_mapping = {
    "microrobotica": (f"{_DEPLOY_BASE}/", None),
    "maddening":     (f"{_DEPLOY_BASE}/maddening/", None),
    "mime":          (f"{_DEPLOY_BASE}/mime/", None),
}
# Don't fetch our own inventory.
intersphinx_mapping.pop(current_project, None)

# ── per-project root document ────────────────────────────────────────
# Sphinx defaults to "index" — both .rst and .md are fine.
master_doc = "index"
root_doc = "index"


# ── preconnect to the third-party CDNs we hit on every page ──────────
# mermaid loads from jsdelivr; sphinx-tippy pulls tippy + popper from
# unpkg. Opening the TLS connection eagerly shaves ~150 ms off the path
# to first interaction on a cold network. Injected into <head> via the
# page context's `metatags` block.
_PRECONNECT_LINKS = (
    '<link rel="preconnect" href="https://cdn.jsdelivr.net" crossorigin>'
    '<link rel="preconnect" href="https://unpkg.com" crossorigin>'
    '<link rel="dns-prefetch" href="https://cdn.jsdelivr.net">'
    '<link rel="dns-prefetch" href="https://unpkg.com">'
)


def _inject_preconnects(app, pagename, templatename, context, doctree):
    context["metatags"] = (context.get("metatags") or "") + _PRECONNECT_LINKS


def setup(app):
    app.connect("html-page-context", _inject_preconnects)

    # sphinxcontrib-mermaid v2 calls `app.add_js_file(d3_url, …)` from its
    # `html-page-context` handler — its `mermaid_d3_zoom` flag only gates
    # the integration script, not d3 itself. d3 is ~91 KB gz / 273 KB raw
    # and we don't use mermaid_d3_zoom. Intercept add_js_file and drop
    # any d3 entry.
    _orig_add_js_file = app.add_js_file
    def _filtered_add_js_file(filename, **kwargs):
        if filename and "/d3@" in filename and filename.endswith(".js"):
            return None
        return _orig_add_js_file(filename, **kwargs)
    app.add_js_file = _filtered_add_js_file

    return {"version": "0.1", "parallel_read_safe": True, "parallel_write_safe": True}
