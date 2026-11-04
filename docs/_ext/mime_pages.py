"""MIME's generated pages (0.3 on): reached by their own links, not a toctree.

`mime docs build` writes `index.md`, a front door by goal that links every
page, and `pages/`, whose pages link one another; it writes no toctree. Every
page but the index is therefore marked an orphan, Sphinx's own declaration
that a document is reached by links, so the build does not warn that each of
them "isn't included in any toctree". The navigation stays MIME's: the index,
the links, the search box and the breadcrumb home.

conf.py loads this only for a MIME build whose sources are such pages.
"""
from __future__ import annotations

from sphinx.application import Sphinx
from sphinx.environment import BuildEnvironment


def _linked_not_listed(app: Sphinx, env: BuildEnvironment) -> None:
    for docname in env.found_docs:
        if docname != app.config.root_doc:
            env.metadata[docname]["orphan"] = True


def setup(app: Sphinx) -> dict[str, object]:
    # env-updated comes after every document is read and before Sphinx checks
    # that each one is in a toctree.
    app.connect("env-updated", _linked_not_listed)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
