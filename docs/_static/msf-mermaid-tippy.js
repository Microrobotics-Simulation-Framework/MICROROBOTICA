/**
 * msf-mermaid-tippy.js — tippy.js glue for the MSF docs.
 *
 * Three jobs, all funnelling into window.tippy():
 *
 *   1. Promote mermaid <title>s (from `click NODE callback "…"`) into tippy
 *      bubbles after sphinxcontrib-mermaid finishes rendering.
 *   2. Apply the `mermaid-tips` directive's sidecar map (see
 *      docs/_ext/mermaid_tips.py) — node + edge tooltips pushed onto
 *      `window.__MSF_MERMAID_TIPS_QUEUE__`, Nth entry → Nth mermaid block.
 *   3. Nested tooltips: sphinx-tippy only wires `{term}` links present at
 *      page load, but glossary definitions contain their own `{term}`
 *      cross-references. We watch for tippy boxes appearing and wire up the
 *      term links inside them — which recurses, since each nested box is
 *      itself observed.
 *
 * Gracefully no-ops if window.tippy is not loaded (sphinx-tippy didn't fire
 * on this page) — leaves the native <title> in place.
 */
(function () {
  "use strict";

  /** sphinx-tippy emits `tippy(link, {...})` without `appendTo`, so by
   *  default tippy anchors the popup inside the link's parent — which means
   *  any tooltip sitting inside a `sd-card` (overflow: hidden) gets clipped.
   *  Override the global default so every popup renders against <body>. */
  function fixTippyDefaults() {
    if (typeof window.tippy !== "function" || !window.tippy.setDefaultProps) {
      return false;
    }
    window.tippy.setDefaultProps({ appendTo: function () { return document.body; } });
    return true;
  }
  if (!fixTippyDefaults()) {
    var tries = 0;
    var iv = setInterval(function () {
      if (fixTippyDefaults() || ++tries > 50) clearInterval(iv);
    }, 100);
  }

  function consumeQueue() {
    var q = window.__MSF_MERMAID_TIPS_QUEUE__ || [];
    window.__MSF_MERMAID_TIPS_DRAINED__ =
      window.__MSF_MERMAID_TIPS_DRAINED__ || [];
    while (q.length) {
      window.__MSF_MERMAID_TIPS_DRAINED__.push(q.shift());
    }
    return window.__MSF_MERMAID_TIPS_DRAINED__;
  }

  function tipsForBlock(blockIndex) {
    return consumeQueue()[blockIndex] || { nodes: {}, edges: {} };
  }

  function attach(target, content) {
    if (!content || !target) return;
    if (typeof window.tippy !== "function") return;
    window.tippy(target, {
      content: String(content),
      allowHTML: true,
      theme: "material",
      maxWidth: 360,
      placement: "auto-end",
      interactive: true,
      arrow: true,
    });
  }

  /** Pull tooltip text from mermaid-emitted <title> and remove the node so the
   *  browser-native tip doesn't overlap with tippy. */
  function harvestTitle(el) {
    var t = el.querySelector(":scope > title");
    if (!t) return null;
    var txt = t.textContent || "";
    t.parentNode.removeChild(t);
    return txt;
  }

  /** Mermaid IDs nodes as `flowchart-<orig>-<seq>` (v11) or `<orig>` (older).
   *  Return the original author-supplied id from a generated DOM id. */
  function nodeKey(domId) {
    var m = /^flowchart-(.+)-\d+$/.exec(domId);
    if (m) return m[1];
    return domId;
  }

  /** Mermaid IDs edges as `L_<src>_<dst>_<ordinal>` (or `L-<src>-<dst>-N`).
   *  Return both the `src->dst` shorthand and the ordinal so callers can match
   *  whichever they declared. */
  function edgeKeys(domId, ordinal) {
    var m = /^L[-_]([^-_]+)[-_]([^-_]+)[-_](\d+)$/.exec(domId || "");
    var src = m ? m[1] : null;
    var dst = m ? m[2] : null;
    return {
      pair: src && dst ? src + "->" + dst : null,
      ordinal: ordinal,
    };
  }

  function decorate(pre, blockIndex) {
    var svg = pre.querySelector("svg");
    if (!svg) return;
    var tips = tipsForBlock(blockIndex);

    // ── nodes ─────────────────────────────────────────────────────
    svg.querySelectorAll("g.node").forEach(function (g) {
      var nativeTip = harvestTitle(g);
      var declaredTip = tips.nodes[nodeKey(g.id)] || tips.nodes[g.id];
      attach(g, declaredTip || nativeTip);
    });

    // ── edges (paths for the line + label group for the label) ────
    var edgePaths = svg.querySelectorAll("path.flowchart-link, g.edgePath > path");
    edgePaths.forEach(function (path, i) {
      // Mermaid attaches the canonical id to the parent <g.edgePath>.
      var parent = path.closest("g.edgePath") || path.parentNode;
      var keys = edgeKeys(parent && parent.id, i);
      var tip =
        (keys.pair && tips.edges[keys.pair]) ||
        tips.edges[String(keys.ordinal)] ||
        tips.edges[keys.ordinal];
      if (tip) {
        // Wider hit area: also attach to the label if mermaid drew one.
        var label = svg.querySelector(
          'g.edgeLabel[id$="' + keys.ordinal + '"]'
        );
        attach(path, tip);
        if (label) attach(label, tip);
        // Make the path easier to hover.
        path.style.pointerEvents = "stroke";
      }
    });
  }

  function isProcessed(pre) {
    return (
      pre.getAttribute("data-processed") === "true" ||
      pre.querySelector("svg") !== null
    );
  }

  /* ── Nested tooltips ──────────────────────────────────────────────────
   * Two sources of definition HTML, keyed by the `term-X` fragment:
   *
   *  - FAST (sync): sphinx-tippy assigns `selector_to_html` as an implicit
   *    global per page — but only for terms referenced on THAT page.
   *  - FULL (async): the glossary page itself defines every term, so we
   *    fetch it once and parse every <dt id="term-X"> + <dd>. This covers
   *    terms that only appear nested inside another definition.
   *
   * `tipNestedTerms` applies the fast index immediately, then the full
   * index when it resolves; the `__msfTipped` guard makes the second pass
   * a no-op for anything already wired.
   */
  function fastTermIndex() {
    var map = window.selector_to_html || {};
    var idx = {};
    Object.keys(map).forEach(function (sel) {
      var m = /#(term-[^"'\]]+)/.exec(sel);
      if (m) idx[m[1]] = map[sel];
    });
    return idx;
  }

  var _glossaryIndex = null; // Promise<{term-X: html}>

  function fullTermIndex() {
    if (_glossaryIndex) return _glossaryIndex;
    // Any {term} link on the page points at the glossary; strip the fragment.
    var seed = document.querySelector('a[href*="glossary.html#term-"], a[href*="/glossary.html#"]');
    if (!seed) {
      _glossaryIndex = Promise.resolve({});
      return _glossaryIndex;
    }
    var url = seed.href.split("#")[0];
    _glossaryIndex = fetch(url)
      .then(function (r) { return r.ok ? r.text() : ""; })
      .then(function (html) {
        var idx = {};
        if (!html) return idx;
        var doc = new DOMParser().parseFromString(html, "text/html");
        doc.querySelectorAll("dl.glossary dt[id]").forEach(function (dt) {
          var dd = dt.nextElementSibling;
          if (!dd || dd.tagName !== "DD") return;
          var dtc = dt.cloneNode(true);
          dtc.querySelectorAll(".headerlink").forEach(function (h) {
            h.parentNode.removeChild(h);
          });
          idx[dt.id] = dtc.outerHTML + dd.outerHTML;
        });
        return idx;
      })
      .catch(function () { return {}; });
    return _glossaryIndex;
  }

  function applyTermTips(root, idx) {
    if (typeof window.tippy !== "function") return;
    root.querySelectorAll('a[href*="#term-"]').forEach(function (a) {
      if (a.__msfTipped) return;
      var m = /#(term-[^"']+)/.exec(a.getAttribute("href") || "");
      if (!m || !idx[m[1]]) return;
      a.__msfTipped = true;
      window.tippy(a, {
        content: idx[m[1]],
        allowHTML: true,
        theme: "material",
        maxWidth: 360,
        placement: "auto-end",
        interactive: true,
        interactiveBorder: 8,
        arrow: true,
      });
    });
  }

  /** Wire a tippy onto every un-tipped `{term}` link inside `root`. */
  function tipNestedTerms(root) {
    if (typeof window.tippy !== "function") return;
    applyTermTips(root, fastTermIndex());
    fullTermIndex().then(function (idx) { applyTermTips(root, idx); });
  }

  /* Watch <body> for tippy boxes mounting. tippy v6 appends a
   * `<div data-tippy-root>` wrapper (we set appendTo: body globally), so we
   * check both the added node and its descendants for `.tippy-box`. A box
   * opened from inside another box is caught the same way → tooltips nest
   * to arbitrary depth. */
  function setupNestedTips() {
    var obs = new MutationObserver(function (mutations) {
      mutations.forEach(function (mut) {
        Array.prototype.forEach.call(mut.addedNodes, function (node) {
          if (node.nodeType !== 1) return;
          if (node.classList && node.classList.contains("tippy-box")) {
            tipNestedTerms(node);
          } else if (node.querySelectorAll) {
            node.querySelectorAll(".tippy-box").forEach(tipNestedTerms);
          }
        });
      });
    });
    obs.observe(document.body, { childList: true, subtree: true });
  }

  function run() {
    var pending = Array.prototype.slice.call(
      document.querySelectorAll("pre.mermaid, div.mermaid")
    );
    if (!pending.length) return;

    // Preserve DOM order so block-index matches the queue order.
    pending.forEach(function (pre, idx) {
      pre.dataset.msfMermaidIndex = String(idx);
      if (isProcessed(pre)) {
        decorate(pre, idx);
        return;
      }
      // sphinxcontrib-mermaid renders asynchronously — watch for the SVG.
      var obs = new MutationObserver(function () {
        if (isProcessed(pre)) {
          obs.disconnect();
          decorate(pre, idx);
        }
      });
      obs.observe(pre, { childList: true, subtree: true, attributes: true });
    });
  }

  /* ── Mobile / touch ───────────────────────────────────────────────────
   * On a touch device a tap on a `{term}` link would just navigate to the
   * glossary — the hover tooltip never gets a chance. Suppress that
   * navigation so a tap instead opens the tippy bubble (tippy's own touch
   * handling shows it on tap and dismisses on tap-outside). Delegated in
   * the capture phase so it also covers term links that only appear later
   * inside an open tooltip. Real mouse users are unaffected. */
  function setupTouchTermLinks() {
    var coarse =
      (window.matchMedia && window.matchMedia("(pointer: coarse)").matches) ||
      "ontouchstart" in window;
    if (!coarse) return;
    document.addEventListener(
      "click",
      function (e) {
        var t = e.target;
        var a = t && t.closest ? t.closest('a[href*="#term-"]') : null;
        if (a) e.preventDefault();
      },
      true
    );
  }

  function init() {
    run();
    setupNestedTips();
    setupTouchTermLinks();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
