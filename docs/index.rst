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
           <button class="msf-hero-fullscreen" type="button"
                   data-target="0" aria-label="Open video fullscreen">
             ⛶ Fullscreen
           </button>
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
           <button class="msf-hero-fullscreen" type="button"
                   data-target="1" aria-label="Open video fullscreen">
             ⛶ Fullscreen
           </button>
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
   user_guide/concepts

.. toctree::
   :hidden:
   :caption: Developer Guide

   developer_guide/index
   developer_guide/component_authoring
   developer_guide/testing_standards

.. toctree::
   :hidden:
   :caption: Component Guide

   component_guide/index

.. toctree::
   :hidden:
   :caption: Validation

   validation/index
   validation/soup_package

.. toctree::
   :hidden:
   :caption: Regulatory

   regulatory/intended_use
   regulatory/downstream_integration
   regulatory/iec62304_mapping
   regulatory/usability_engineering
   regulatory/eu_mdr_guidelines
   regulatory/mdcg_2019_11
