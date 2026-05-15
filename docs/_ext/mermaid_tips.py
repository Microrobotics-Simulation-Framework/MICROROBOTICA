"""``mermaid-tips`` directive — sidecar tooltip map for mermaid diagrams.

Place this fence *immediately before* a ``mermaid`` block. The YAML body
declares per-node and per-edge tooltips that ``_static/msf-mermaid-tippy.js``
attaches as tippy instances after mermaid finishes rendering.

Example
-------
::

    :::{mermaid-tips}
    nodes:
      A: Rigid body kinematics
      B: Permanent magnet response
    edges:
      A->B: Orientation vector passed each step
      0:    Magnetic force feedback
    :::

    :::{mermaid}
    flowchart LR
      A --> B
    :::

The directive emits a tiny ``<script>`` that pushes the parsed map onto a
shared FIFO (``window.__MSF_MERMAID_TIPS_QUEUE__``). The JS shim drains the
queue in DOM order, matching each entry to the next mermaid block on the page.
"""

from __future__ import annotations

import json

import yaml
from docutils import nodes
from docutils.parsers.rst import Directive


class MermaidTipsDirective(Directive):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    final_argument_whitespace = False
    option_spec = {}

    def run(self):
        try:
            data = yaml.safe_load("\n".join(self.content)) or {}
        except yaml.YAMLError as exc:
            error = self.state_machine.reporter.error(
                f"mermaid-tips: invalid YAML ({exc})",
                nodes.literal_block(self.block_text, self.block_text),
                line=self.lineno,
            )
            return [error]

        if not isinstance(data, dict):
            data = {}

        payload = json.dumps(
            {"nodes": data.get("nodes") or {}, "edges": data.get("edges") or {}}
        )
        html = (
            "<script>"
            "(window.__MSF_MERMAID_TIPS_QUEUE__=window.__MSF_MERMAID_TIPS_QUEUE__||[])"
            f".push({payload});"
            "</script>"
        )
        return [nodes.raw("", html, format="html")]


def setup(app):
    app.add_directive("mermaid-tips", MermaidTipsDirective)
    return {"version": "0.1", "parallel_read_safe": True, "parallel_write_safe": True}
