:html_theme.sidebar_secondary.remove:

Microrobotics Simulation Framework
===================================

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
                aria-label="MIME replication of de Jongh 2025 helical-UMR experiment">
           <source src="videos/dejongh_mime_replication_demo.mp4" type="video/mp4">
         </video>
         <div class="msf-hero-caption">
           MIME replication of the de Jongh et al. (2025) helical-UMR
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
       _mermaidPromise = import('https://cdn.jsdelivr.net/npm/mermaid@11.15.0/dist/mermaid.esm.min.mjs')
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

.. rst-class:: msf-hero-blurb

An end-to-end, autodifferentiable simulation framework for
**magnetically actuated microrobots** in confined biological flows.
Implemented on top of a modular graphs-based physics system that couples
:term:`low-Reynolds <Low-Re>` hydrodynamics, :term:`magnetic response
<Dipole response>`, :term:`Stokeslet`-based drag, and robot
kinematics/dynamics through closed-loop control — all of it
:term:`autodifferentiable <JAX>` end-to-end and wired into a regulated IDE.

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

Architecture
------------

The stack is intentionally one-way: the runtime knows nothing about the
domain, the domain knows nothing about the IDE, and the IDE knows nothing
about a downstream commercial product. Each layer also emits *metadata*
that flows orthogonally into a shared regulatory audit trail — so the
IEC 62304 evidence at the bottom of the pipe doesn't appear from nowhere.

.. mermaid-tips::

   nodes:
     MAD:   MADDENING — autodifferentiable JAX graph runtime. Knows nothing about physics or medical regulation.
     MIM:   MIME — microrobotics physics nodes and control primitives on top of MADDENING, plus all the domain metadata.
     MR:    MICROROBOTICA — Qt IDE + asset registry. Loads MIME experiments, renders trajectories, owns the audit trail.
     META:  The metadata side-channel — biocompatibility, anatomical regime, SOUP class, verification status, anomaly logs. Each layer contributes; nothing is added by hand at the end.
     AUDIT: IEC 62304 / EU MDR evidence package. Auto-assembled from the metadata above; a downstream device adopts it wholesale.
     PROD:  Downstream CE-marked device — the framework supplies V&V evidence, the product carries the regulatory approval.
   edges:
     MAD->MIM:   MIME subclasses MADDENING SimulationNode and adds microrobotics-specific physics + domain metadata.
     MIM->MR:    MICROROBOTICA loads each MIME asset (graph + schema + benchmarks) and replays the recorded USD trajectory.
     MAD->META:  MADDENING contributes a CycloneDX SBOM and verification-harness results for every node.
     MIM->META:  MIME's MimeAssetSchema bundles biocompatibility, ISO 14971 hazards, anatomical regime, and B0-B5 benchmark results.
     MR->META:   MICROROBOTICA stamps every interface with ComponentMeta and aggregates the anomaly registry.
     META->AUDIT: The audit package is assembled mechanically from the metadata above — no manual transcription, no surprise.
     AUDIT->PROD: A downstream device inherits the audit trail; its 62304 submission references this evidence package.

.. mermaid::

   flowchart LR
       MAD["MADDENING<br/><i>graph runtime &middot; autodiff &middot; surrogates</i>"]
       MIM["MIME<br/><i>physics nodes &middot; control &middot; UQ</i>"]
       MR["MICROROBOTICA<br/><i>Qt IDE &middot; registry</i>"]

       MAD --> MIM
       MIM --> MR

       META(["regulatory metadata<br/>biocompat &middot; SOUP &middot; verification &middot; anomalies"]):::meta
       AUDIT[["IEC 62304 / EU MDR<br/>audit package"]]:::audit
       PROD["CE-marked device"]:::ext

       MAD -. SBOM + V&V .-> META
       MIM -. AssetSchema .-> META
       MR  -. ComponentMeta .-> META
       META --> AUDIT
       AUDIT --> PROD

       click MAD href "maddening/"
       click MIM href "mime/"
       click MR href "user_guide/index.html"

       classDef meta stroke-width:1.5px
       classDef audit stroke-width:1.5px
       classDef ext stroke-dasharray:5 3

How MICROROBOTICA is wired internally
-------------------------------------

The IDE is a Qt 6 / C++17 desktop application with an embedded Python
console. Its job is to load a MIME experiment, drive it through a
non-blocking ``PhysicsProcess``, and let the user scrub the resulting USD
trajectory — all while leaving an audit trail.

.. mermaid-tips::

   nodes:
     Panels:   Qt dock widgets — hierarchy, properties, timeline, console. The user-facing surface.
     Viewport: OpenGL viewport (or a software fallback for headless / Docker). Lazily created so the app survives without a GPU.
     Scene:    USD three-layer composition. The anatomy base layer is immutable; the simulator writes to a results layer; user edits live in an override layer.
     Sim:      SimulationController — owns the simulation lifecycle and a thread-safe queue that decouples blocking physics from the 60 Hz Qt event loop.
     Phys:     PhysicsProcess — the abstract interface MIME (or any other backend) implements. Runs the actual graph step on a worker thread or remote VM.
     Script:   Embedded Python console. Same `microrobotica` pybind11 module as the future standalone library — single-source.
     Core:     Pure C++17 core. Zero Qt, USD, or Python dependencies — this is where ComponentMeta and the verification benchmarks live.
     MIM:      MIME process (separate Python interpreter) driven by MICROROBOTICA over the PhysicsProcess interface.
   edges:
     Panels->Sim:    User actions (load, play, scrub, parameter edit) post commands onto the simulation queue.
     Sim->Phys:      Each tick the controller hands the next dt and current state to the active PhysicsProcess.
     Phys->Sim:      Results arrive asynchronously and are merged back into the scene's results layer for rendering.
     Scene->Viewport: The viewport draws whatever the active USD stage composes, regardless of where it came from.
     Sim->Core:      Every simulation interface is annotated with ComponentMeta so the audit log can name what ran and when.
     Phys->MIM:      The default PhysicsProcess implementation shells out to a MIME runner over the schema.

.. mermaid::

   flowchart LR
       subgraph QT["Qt application shell"]
         direction TB
         Panels["panels<br/><i>hierarchy &middot; properties<br/>timeline &middot; console</i>"]
         Viewport["viewport<br/><i>OpenGL / software</i>"]
       end

       subgraph CORE["core (no Qt / USD / Python)"]
         direction TB
         ICore["interfaces<br/>ComponentMeta"]
         DCore["typed data &middot; benchmarks"]
       end

       Scene["scene<br/><i>USD three-layer composition</i>"]
       Sim["simulation<br/><i>SimulationController<br/>async result queue</i>"]
       Script["scripting<br/><i>embedded Python<br/>(pybind11)</i>"]
       Phys["PhysicsProcess<br/><i>abstract backend</i>"]:::iface
       MIM["MIME runner"]:::ext

       Panels --> Sim
       Panels --> Scene
       Sim --> Phys
       Phys --> Sim
       Phys --> MIM
       Scene --> Viewport
       Scene -. USD stages .-> Phys
       Sim --> ICore
       Scene --> ICore
       Script --> ICore
       Script --> Sim

       classDef ext stroke-dasharray:5 3
       classDef iface stroke-width:1.5px

Documentation map
-----------------

The framework's documentation is split across the three projects' own
docs trees, which all share this site's theme. Use the navbar to jump
between them. A shared :doc:`glossary` defines the recurring jargon.

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

.. toctree::
   :hidden:
   :caption: Reference

   glossary

.. note::

   *Status: scaffolding.* Most pages are placeholders and will fill in
   over the coming weeks. Source for every page lives in the project
   repos linked in the cards above — contributions welcome.
