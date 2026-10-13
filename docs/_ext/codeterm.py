"""``codeterm`` role: a glossary-term reference that keeps its code styling.

``{term}`` renders its text as plain prose. A term spelled the way the code
spells it (``ParamSpec``, ``STABLE``) reads best in code style, so
``{codeterm}`` resolves exactly as ``{term}`` does (Sphinx's ``std:term``)
and renders the text as inline code inside the same link::

    {codeterm}`ParamSpec`
    {codeterm}`StabilityLevel.STABLE <STABLE>`

The HTML is ``<a class="reference internal" href="...#term-ParamSpec">``
around ``<code class="xref std std-term docutils literal">``, so the
sphinx-tippy tooltip (which keys on the link's ``#term-`` href) and the
term-link CSS in ``_static/custom.css`` apply unchanged. A name missing from
the glossary warns as ``{term}`` does ("term not in glossary").
"""

from __future__ import annotations

from docutils import nodes
from sphinx.roles import XRefRole


class CodeTermRole(XRefRole):
    """``std:term`` cross-reference whose inner node is a literal."""

    def run(self):
        # XRefRole derives the domain, the reference type and the CSS classes
        # from the role's name; resolve as Sphinx's own glossary role.
        self.name = "std:term"
        return super().run()


def setup(app):
    app.add_role(
        "codeterm",
        CodeTermRole(innernodeclass=nodes.literal, warn_dangling=True),
    )
    return {
        "version": "0.1",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
