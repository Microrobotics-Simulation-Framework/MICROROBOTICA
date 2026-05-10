:html_theme.sidebar_secondary.remove:

Microrobotics Simulation Framework
===================================

.. admonition:: Scaffolding — site under active construction
   :class: warning

   This site is documentation **scaffolding**: structure is in place
   but most pages will be filled in over the coming weeks. Expect
   gaps, placeholder sections, and the occasional broken cross-reference.
   The source for every page lives in the project repos linked below —
   contributions welcome.

.. raw:: html

   <!-- Mermaid diagram modal overlay -->
   <div id="msf-diagram-modal" class="msf-diagram-modal" role="dialog" aria-modal="true" aria-label="Architecture diagram" hidden>
     <div class="msf-diagram-modal-backdrop"></div>
     <div class="msf-diagram-modal-panel">
       <button class="msf-diagram-modal-close" aria-label="Close diagram">&times;</button>
       <button class="msf-diagram-modal-expand" aria-label="Toggle fullscreen">⛶</button>
       <h3 class="msf-diagram-modal-title" id="msf-diagram-title"></h3>
       <div class="msf-diagram-modal-body" id="msf-diagram-body"></div>
     </div>
   </div>

   <div class="msf-hero">
     <div class="msf-hero-videos">
       <div class="msf-hero-video">
         <video controls autoplay muted loop playsinline preload="metadata"
                aria-label="MICROROBOTICA dipole + robot-arm closed-loop control">
           <source src="videos/microrobot_dipole_robot_arm_control.mp4" type="video/mp4">
         </video>
         <div class="msf-hero-caption">
           Closed-loop control of a magnetically actuated microrobot
           tracked by a robot-arm-mounted dipole field source.
           <span class="msf-hero-actions">
             <button class="msf-hero-fullscreen" type="button"
                     data-target="0" aria-label="Open video fullscreen">
               ⛶ Fullscreen
             </button>
             <button class="msf-hero-diagram" type="button"
                     data-diagram="dipole-arm"
                     aria-label="View architecture diagram">
               &#128202; Diagram
             </button>
           </span>
         </div>
       </div>
       <div class="msf-hero-video">
         <video controls autoplay muted loop playsinline preload="metadata"
                aria-label="MIME replication of de Jongh 2024 helical-UMR experiment">
           <source src="videos/dejongh_mime_replication_demo.mp4" type="video/mp4">
         </video>
         <div class="msf-hero-caption">
           MIME replication of the de Jongh et al. (2024) helical-UMR
           propulsion experiment, end-to-end inside the framework.
           <span class="msf-hero-actions">
             <button class="msf-hero-fullscreen" type="button"
                     data-target="1" aria-label="Open video fullscreen">
               ⛶ Fullscreen
             </button>
             <button class="msf-hero-diagram" type="button"
                     data-diagram="dejongh-mime"
                     aria-label="View architecture diagram">
               &#128202; Diagram
             </button>
           </span>
         </div>
       </div>
     </div>
     <p class="msf-hero-blurb">
       An end-to-end, autodifferentiable simulation framework for
       <strong>magnetically actuated microrobots</strong> in confined
       biological flows — from acausal node graphs and low-Reynolds
       hydrodynamics, through closed-loop control, to a regulated IDE.
     </p>
   </div>

   <script>
   /* ── Fullscreen handler ─────────────────────────────────────────── */
   document.querySelectorAll('.msf-hero-fullscreen').forEach(function (btn) {
     btn.addEventListener('click', function () {
       const card = btn.closest('.msf-hero-video');
       const video = card && card.querySelector('video');
       if (!video) return;
       const req = video.requestFullscreen
                || video.webkitRequestFullscreen
                || video.msRequestFullscreen;
       if (req) req.call(video);
     });
   });

   /* ── Mermaid diagram modal ──────────────────────────────────────── */
   /* Diagram data lives in _static/msf-diagrams.js (not inline) to   */
   /* avoid Sphinx HTML-encoding the --> arrows in JS strings.         */
   (function () {
     var modal   = document.getElementById('msf-diagram-modal');
     var title   = document.getElementById('msf-diagram-title');
     var body    = document.getElementById('msf-diagram-body');
     var closeBtn = modal && modal.querySelector('.msf-diagram-modal-close');
     var expandBtn = modal && modal.querySelector('.msf-diagram-modal-expand');
     var backdrop = modal && modal.querySelector('.msf-diagram-modal-backdrop');
     if (!modal) return;

     /* Lazily loaded mermaid instance — cached after first open */
     var _mermaidPromise = null;
     function getMermaid() {
       if (_mermaidPromise) return _mermaidPromise;
       _mermaidPromise = import('https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.esm.min.mjs')
         .then(function (mod) {
           var m = mod.default || mod;
           var isDark = document.documentElement.dataset.theme === 'dark';
           m.initialize({ startOnLoad: false, theme: isDark ? 'dark' : 'default' });
           return m;
         });
       return _mermaidPromise;
     }

     function openModal(key) {
       var diagrams = window.MSF_DIAGRAMS || {};
       var d = diagrams[key];
       if (!d) return;
       title.textContent = d.title;
       body.innerHTML = '<pre class="mermaid">' + d.graph + '</pre>';
       modal.hidden = false;
       document.body.classList.add('msf-modal-open');
       getMermaid().then(function (mermaid) {
         mermaid.run({ nodes: body.querySelectorAll('.mermaid') }).then(function () {
           if (window.msf && window.msf.mermaidZoom) {
             window.msf.mermaidZoom(body);
           }
         });
       });
     }

     function closeModal() {
       modal.hidden = true;
       modal.classList.remove('msf-diagram-modal-fullscreen');
       document.body.classList.remove('msf-modal-open');
     }

     document.querySelectorAll('.msf-hero-diagram').forEach(function (btn) {
       btn.addEventListener('click', function () {
         openModal(btn.getAttribute('data-diagram'));
       });
     });

     closeBtn && closeBtn.addEventListener('click', closeModal);
     expandBtn && expandBtn.addEventListener('click', function() {
       modal.classList.toggle('msf-diagram-modal-fullscreen');
     });
     backdrop && backdrop.addEventListener('click', closeModal);
     document.addEventListener('keydown', function (e) {
       if (e.key === 'Escape' && !modal.hidden) closeModal();
     });
   })();
   </script>

The framework is three layered projects:

.. grid:: 1 2 3 3
   :gutter: 3
   :margin: 4 4 0 0

   .. grid-item-card:: MADDENING
      :link: ../maddening/
      :class-card: msf-card

      The base framework. A pure-JAX, autodifferentiable
      acausal-dataflow runtime where physical models are composed from
      typed nodes and unit-aware edges.

      +++

      `Repo on GitHub <https://github.com/Microrobotics-Simulation-Framework/MADDENING>`__

   .. grid-item-card:: MIME
      :link: ../mime/
      :class-card: msf-card

      The physics. A library of MADDENING nodes for rigid-body chains,
      magnetic response, low-Reynolds hydrodynamics
      (Stokeslet / IBM-FVM with optional GNN correction), and the
      actuation chain.

      +++

      `Repo on GitHub <https://github.com/Microrobotics-Simulation-Framework/MIME>`__

   .. grid-item-card:: MICROROBOTICA
      :link: user_guide/index
      :link-type: doc
      :class-card: msf-card

      The IDE. A Qt application that loads MIME experiments, drives
      live parameter edits, scrubs USD recordings, and produces the
      IEC-62304 audit trail for clinical-grade adoption.

      +++

      `Repo on GitHub <https://github.com/Microrobotics-Simulation-Framework/MICROROBOTICA>`__

Documentation map
-----------------

The framework's documentation is split across the three projects' own
docs trees, which all share this site's theme. Use the navbar to jump
between them.

.. toctree::
   :hidden:
   :caption: User Guide

   user_guide/index

.. toctree::
   :hidden:
   :caption: Developer Guide

   developer_guide/index

.. toctree::
   :hidden:
   :caption: Component Guide

   component_guide/index

.. toctree::
   :hidden:
   :caption: Validation

   validation/index

.. toctree::
   :hidden:
   :caption: Regulatory

   regulatory/index
