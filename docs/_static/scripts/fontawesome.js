/*!
 * MSF slim FontAwesome shim — overrides pydata-sphinx-theme's bundled
 * fontawesome.js (~540 KB gz / 1.4 MB raw) with a hand-rolled SVG
 * inliner that only carries the 12 icons the rendered site actually
 * uses. Total payload ~5 KB.
 *
 * Drops in transparently: the API surface we care about is `<i class="fa-…">`
 * → swapped for `<svg>` at DOMContentLoaded, plus a MutationObserver
 * for icons added later (theme switcher, dropdown menus). All classes
 * on the source `<i>` carry through to the `<svg>`, so pydata's own CSS
 * (e.g. `.theme-switch.fa-sun`) keeps working.
 */
(function () {
  "use strict";

  /* Path data extracted from @fortawesome/fontawesome-free@6.6.0. */
  var ICONS = {
    "angle-right":         {p:"fas",v:"0 0 320 512",d:"M278.6 233.4c12.5 12.5 12.5 32.8 0 45.3l-160 160c-12.5 12.5-32.8 12.5-45.3 0s-12.5-32.8 0-45.3L210.7 256 73.4 118.6c-12.5-12.5-12.5-32.8 0-45.3s32.8-12.5 45.3 0l160 160z"},
    "arrow-up":            {p:"fas",v:"0 0 384 512",d:"M214.6 41.4c-12.5-12.5-32.8-12.5-45.3 0l-160 160c-12.5 12.5-12.5 32.8 0 45.3s32.8 12.5 45.3 0L160 141.2 160 448c0 17.7 14.3 32 32 32s32-14.3 32-32l0-306.7L329.4 246.6c12.5 12.5 32.8 12.5 45.3 0s12.5-32.8 0-45.3l-160-160z"},
    "bars":                {p:"fas",v:"0 0 448 512",d:"M0 96C0 78.3 14.3 64 32 64l384 0c17.7 0 32 14.3 32 32s-14.3 32-32 32L32 128C14.3 128 0 113.7 0 96zM0 256c0-17.7 14.3-32 32-32l384 0c17.7 0 32 14.3 32 32s-14.3 32-32 32L32 288c-17.7 0-32-14.3-32-32zM448 416c0 17.7-14.3 32-32 32L32 448c-17.7 0-32-14.3-32-32s14.3-32 32-32l384 0c17.7 0 32 14.3 32 32z"},
    "cubes":               {p:"fas",v:"0 0 576 512",d:"M290.8 48.6l78.4 29.7L288 109.5 206.8 78.3l78.4-29.7c1.8-.7 3.8-.7 5.7 0zM136 92.5l0 112.2c-1.3 .4-2.6 .8-3.9 1.3l-96 36.4C14.4 250.6 0 271.5 0 294.7L0 413.9c0 22.2 13.1 42.3 33.5 51.3l96 42.2c14.4 6.3 30.7 6.3 45.1 0L288 457.5l113.5 49.9c14.4 6.3 30.7 6.3 45.1 0l96-42.2c20.3-8.9 33.5-29.1 33.5-51.3l0-119.1c0-23.3-14.4-44.1-36.1-52.4l-96-36.4c-1.3-.5-2.6-.9-3.9-1.3l0-112.2c0-23.3-14.4-44.1-36.1-52.4l-96-36.4c-12.8-4.8-26.9-4.8-39.7 0l-96 36.4C150.4 48.4 136 69.3 136 92.5zM392 210.6l-82.4 31.2 0-89.2L392 121l0 89.6zM154.8 250.9l78.4 29.7L152 311.7 70.8 280.6l78.4-29.7c1.8-.7 3.8-.7 5.7 0zm18.8 204.4l0-100.5L256 323.2l0 95.9-82.4 36.2zM421.2 250.9c1.8-.7 3.8-.7 5.7 0l78.4 29.7L424 311.7l-81.2-31.1 78.4-29.7zM523.2 421.2l-77.6 34.1 0-100.5L528 323.2l0 90.7c0 3.2-1.9 6-4.8 7.3z"},
    "file-lines":          {p:"fas",v:"0 0 384 512",d:"M64 0C28.7 0 0 28.7 0 64L0 448c0 35.3 28.7 64 64 64l256 0c35.3 0 64-28.7 64-64l0-288-128 0c-17.7 0-32-14.3-32-32L224 0 64 0zM256 0l0 128 128 0L256 0zM112 256l160 0c8.8 0 16 7.2 16 16s-7.2 16-16 16l-160 0c-8.8 0-16-7.2-16-16s7.2-16 16-16zm0 64l160 0c8.8 0 16 7.2 16 16s-7.2 16-16 16l-160 0c-8.8 0-16-7.2-16-16s7.2-16 16-16zm0 64l160 0c8.8 0 16 7.2 16 16s-7.2 16-16 16l-160 0c-8.8 0-16-7.2-16-16s7.2-16 16-16z"},
    "list":                {p:"fas",v:"0 0 512 512",d:"M40 48C26.7 48 16 58.7 16 72l0 48c0 13.3 10.7 24 24 24l48 0c13.3 0 24-10.7 24-24l0-48c0-13.3-10.7-24-24-24L40 48zM192 64c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32L192 64zm0 160c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32l-288 0zm0 160c-17.7 0-32 14.3-32 32s14.3 32 32 32l288 0c17.7 0 32-14.3 32-32s-14.3-32-32-32l-288 0zM16 232l0 48c0 13.3 10.7 24 24 24l48 0c13.3 0 24-10.7 24-24l0-48c0-13.3-10.7-24-24-24l-48 0c-13.3 0-24 10.7-24 24zM40 368c-13.3 0-24 10.7-24 24l0 48c0 13.3 10.7 24 24 24l48 0c13.3 0 24-10.7 24-24l0-48c0-13.3-10.7-24-24-24l-48 0z"},
    "magnifying-glass":    {p:"fas",v:"0 0 512 512",d:"M416 208c0 45.9-14.9 88.3-40 122.7L502.6 457.4c12.5 12.5 12.5 32.8 0 45.3s-32.8 12.5-45.3 0L330.7 376c-34.4 25.2-76.8 40-122.7 40C93.1 416 0 322.9 0 208S93.1 0 208 0S416 93.1 416 208zM208 352a144 144 0 1 0 0-288 144 144 0 1 0 0 288z"},
    "outdent":             {p:"fas",v:"0 0 448 512",d:"M0 64C0 46.3 14.3 32 32 32l384 0c17.7 0 32 14.3 32 32s-14.3 32-32 32L32 96C14.3 96 0 81.7 0 64zM192 192c0-17.7 14.3-32 32-32l192 0c17.7 0 32 14.3 32 32s-14.3 32-32 32l-192 0c-17.7 0-32-14.3-32-32zm32 96l192 0c17.7 0 32 14.3 32 32s-14.3 32-32 32l-192 0c-17.7 0-32-14.3-32-32s14.3-32 32-32zM0 448c0-17.7 14.3-32 32-32l384 0c17.7 0 32 14.3 32 32s-14.3 32-32 32L32 480c-17.7 0-32-14.3-32-32zM.2 268.6c-8.2-6.4-8.2-18.9 0-25.3l101.9-79.3c10.5-8.2 25.8-.7 25.8 12.6l0 158.6c0 13.3-15.3 20.8-25.8 12.6L.2 268.6z"},
    "circle-half-stroke":  {p:"fas",v:"0 0 512 512",d:"M448 256c0-106-86-192-192-192l0 384c106 0 192-86 192-192zM0 256a256 256 0 1 1 512 0A256 256 0 1 1 0 256z"},
    "moon":                {p:"fas",v:"0 0 384 512",d:"M223.5 32C100 32 0 132.3 0 256S100 480 223.5 480c60.6 0 115.5-24.2 155.8-63.4c5-4.9 6.3-12.5 3.1-18.7s-10.1-9.7-17-8.5c-9.8 1.7-19.8 2.6-30.1 2.6c-96.9 0-175.5-78.8-175.5-176c0-65.8 36-123.1 89.3-153.3c6.1-3.5 9.2-10.5 7.7-17.3s-7.3-11.9-14.3-12.5c-6.3-.5-12.6-.8-19-.8z"},
    "sun":                 {p:"fas",v:"0 0 512 512",d:"M361.5 1.2c5 2.1 8.6 6.6 9.6 11.9L391 121l107.9 19.8c5.3 1 9.8 4.6 11.9 9.6s1.5 10.7-1.6 15.2L446.9 256l62.3 90.3c3.1 4.5 3.7 10.2 1.6 15.2s-6.6 8.6-11.9 9.6L391 391 371.1 498.9c-1 5.3-4.6 9.8-9.6 11.9s-10.7 1.5-15.2-1.6L256 446.9l-90.3 62.3c-4.5 3.1-10.2 3.7-15.2 1.6s-8.6-6.6-9.6-11.9L121 391 13.1 371.1c-5.3-1-9.8-4.6-11.9-9.6s-1.5-10.7 1.6-15.2L65.1 256 2.8 165.7c-3.1-4.5-3.7-10.2-1.6-15.2s6.6-8.6 11.9-9.6L121 121 140.9 13.1c1-5.3 4.6-9.8 9.6-11.9s10.7-1.5 15.2 1.6L256 65.1 346.3 2.8c4.5-3.1 10.2-3.7 15.2-1.6zM160 256a96 96 0 1 1 192 0 96 96 0 1 1 -192 0zm224 0a128 128 0 1 0 -256 0 128 128 0 1 0 256 0z"},
    "github":              {p:"fab",v:"0 0 496 512",d:"M165.9 397.4c0 2-2.3 3.6-5.2 3.6-3.3.3-5.6-1.3-5.6-3.6 0-2 2.3-3.6 5.2-3.6 3-.3 5.6 1.3 5.6 3.6zm-31.1-4.5c-.7 2 1.3 4.3 4.3 4.9 2.6 1 5.6 0 6.2-2s-1.3-4.3-4.3-5.2c-2.6-.7-5.5.3-6.2 2.3zm44.2-1.7c-2.9.7-4.9 2.6-4.6 4.9.3 2 2.9 3.3 5.9 2.6 2.9-.7 4.9-2.6 4.6-4.6-.3-1.9-3-3.2-5.9-2.9zM244.8 8C106.1 8 0 113.3 0 252c0 110.9 69.8 205.8 169.5 239.2 12.8 2.3 17.3-5.6 17.3-12.1 0-6.2-.3-40.4-.3-61.4 0 0-70 15-84.7-29.8 0 0-11.4-29.1-27.8-36.6 0 0-22.9-15.7 1.6-15.4 0 0 24.9 2 38.6 25.8 21.9 38.6 58.6 27.5 72.9 20.9 2.3-16 8.8-27.1 16-33.7-55.9-6.2-112.3-14.3-112.3-110.5 0-27.5 7.6-41.3 23.6-58.9-2.6-6.5-11.1-33.3 2.6-67.9 20.9-6.5 69 27 69 27 20-5.6 41.5-8.5 62.8-8.5s42.8 2.9 62.8 8.5c0 0 48.1-33.6 69-27 13.7 34.7 5.2 61.4 2.6 67.9 16 17.7 25.8 31.5 25.8 58.9 0 96.5-58.9 104.2-114.8 110.5 9.2 7.9 17 22.9 17 46.4 0 33.7-.3 75.4-.3 83.6 0 6.5 4.6 14.4 17.3 12.1C428.2 457.8 496 362.9 496 252 496 113.3 383.5 8 244.8 8z"}
  };

  /* Family selector → class prefix used in markup. */
  var FAMILY = { fas: "fa-solid", fab: "fa-brands", far: "fa-regular" };
  var SIZE_CLASSES = /^fa-(?:xs|sm|lg|xl|2xl|2x|3x|4x|5x|6x|7x|8x|9x|10x|fw|pull-left|pull-right|spin|pulse|fixed-width|rotate-[\dt]+|flip-[hv]+)$/;

  function buildSvg(name, iEl) {
    var data = ICONS[name];
    if (!data) return null;
    var svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    var classes = ["svg-inline--fa", "fa-" + name];
    /* Carry every non-FA-grammar class from the <i> through to the SVG
     * — keeps `.theme-switch`, custom utility classes etc. wired. */
    iEl.classList.forEach(function (c) {
      if (c === "fa-solid" || c === "fa-brands" || c === "fa-regular") return;
      if (c === "fa-" + name) return;
      classes.push(c);
    });
    svg.setAttribute("class", classes.join(" "));
    svg.setAttribute("aria-hidden", "true");
    svg.setAttribute("role", "img");
    svg.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    svg.setAttribute("viewBox", data.v);
    svg.setAttribute("data-prefix", data.p);
    svg.setAttribute("data-icon", name);
    svg.setAttribute("data-fa-i2svg", "");
    /* Mirror the source's id, aria-label, and every data-* attribute
     * onto the SVG — pydata-sphinx-theme keys its theme-switcher CSS off
     * `[data-mode=…]` on the icon, for example. */
    if (iEl.id) svg.id = iEl.id;
    var label = iEl.getAttribute("aria-label") || iEl.getAttribute("title");
    if (label) svg.setAttribute("aria-label", label);
    for (var ai = 0; ai < iEl.attributes.length; ai++) {
      var attr = iEl.attributes[ai];
      if (attr.name.indexOf("data-") === 0) svg.setAttribute(attr.name, attr.value);
    }
    var path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("fill", "currentColor");
    path.setAttribute("d", data.d);
    svg.appendChild(path);
    return svg;
  }

  function iconNameFor(iEl) {
    var found = null;
    for (var i = 0; i < iEl.classList.length; i++) {
      var c = iEl.classList[i];
      if (c === "fa-solid" || c === "fa-brands" || c === "fa-regular") continue;
      if (SIZE_CLASSES.test(c)) continue;
      if (c.indexOf("fa-") === 0) {
        var n = c.slice(3);
        if (ICONS[n]) { found = n; break; }
      }
    }
    return found;
  }

  function isFaSource(node) {
    return node.nodeType === 1 && node.tagName === "I" &&
      (node.classList.contains("fa-solid") ||
       node.classList.contains("fa-brands") ||
       node.classList.contains("fa-regular"));
  }

  function replaceIcons(root) {
    if (!root || !root.querySelectorAll) return;
    var els = root.querySelectorAll("i.fa-solid, i.fa-brands, i.fa-regular");
    for (var i = 0; i < els.length; i++) {
      var el = els[i];
      if (el.dataset.faProcessed) continue;
      var name = iconNameFor(el);
      if (!name) continue;
      var svg = buildSvg(name, el);
      if (!svg) continue;
      el.parentNode.replaceChild(svg, el);
    }
    if (isFaSource(root)) {
      var name2 = iconNameFor(root);
      if (name2) {
        var svg2 = buildSvg(name2, root);
        if (svg2 && root.parentNode) root.parentNode.replaceChild(svg2, root);
      }
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", function () { replaceIcons(document); });
  } else {
    replaceIcons(document);
  }

  /* pydata adds icons dynamically (theme switcher, dropdowns) so keep
   * an observer running for the page's lifetime — it's cheap. */
  var obs = new MutationObserver(function (muts) {
    for (var i = 0; i < muts.length; i++) {
      var added = muts[i].addedNodes;
      for (var j = 0; j < added.length; j++) replaceIcons(added[j]);
    }
  });
  if (document.body) {
    obs.observe(document.body, { childList: true, subtree: true });
  } else {
    document.addEventListener("DOMContentLoaded", function () {
      obs.observe(document.body, { childList: true, subtree: true });
    });
  }

  /* Mark the page so pydata's own JS (which probes for FontAwesome) sees
   * something installed. */
  document.documentElement.classList.add("fontawesome-i2svg-active", "fontawesome-i2svg-complete");
})();
