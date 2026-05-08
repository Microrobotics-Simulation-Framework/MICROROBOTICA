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
from pathlib import Path

# ── multiproject ─────────────────────────────────────────────────────
from multiproject.utils import get_project

multiproject_projects = {
    "microrobotica": {"path": "."},
    "maddening":     {"path": "projects/maddening/docs"},
    "mime":          {"path": "projects/mime/docs"},
}

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
]

# Breathe (C++ via Doxygen XML) only for MICROROBOTICA.
if current_project == "microrobotica":
    extensions.append("breathe")
    breathe_projects = {"MICROROBOTICA": "_doxygen/xml"}
    breathe_default_project = "MICROROBOTICA"

# MyST extensions.
myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "html_image",
    "linkify",
    "substitution",
]
myst_heading_anchors = 3

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
html_css_files = ["custom.css"]

# Copy `assets/` (videos and other large media kept out of _static/) to
# the build output as-is. Only on the microrobotica project — the
# subprojects do not currently have assets to ship.
if current_project == "microrobotica":
    html_extra_path = ["assets"]
html_logo = None  # text logo from theme_options below
html_favicon = None

# When deployed on GitHub Pages, each subproject lives at /<name>/.
# We tell pydata-sphinx-theme where each one lives so its sidebar and
# breadcrumbs render correct cross-project links.
_DEPLOY_BASE = os.environ.get("DOCS_DEPLOY_BASE", "https://microrobotica.org")

html_theme_options = {
    "logo": {
        "text": "Microrobotics Simulation Framework",
    },
    "navbar_start": ["navbar-logo"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["theme-switcher", "navbar-icon-links"],
    "navbar_persistent": ["search-button"],
    "navbar_align": "left",
    "show_nav_level": 2,
    "navigation_depth": 4,
    "show_toc_level": 2,
    "header_links_before_dropdown": 5,
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
        f"<a href=\"{_repo_url}\" class=\"reference external\">Source on GitHub →</a> "
        "&middot; <em>Docs are scaffolding — pages will fill in over the coming weeks.</em></p>"
    ),
    "footer_start": ["copyright"],
    "footer_end": ["theme-version"],
}

html_context = {
    "current_project": current_project,
    "deploy_base": _DEPLOY_BASE,
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
