/**
 * MSF hero diagram data for the mermaid modal on the landing page.
 * This file is served as-is (html_static_path) so Sphinx never HTML-encodes it.
 */
/* Both diagrams below mirror the node graph wired up by the
 * corresponding MIME experiment (graph.json + physics/setup.py):
 *   "dipole-arm"   -> MIME/experiments/ar4_helical_drive/
 *   "dejongh-mime" -> MIME/experiments/dejongh_confined/
 * Keep these in sync with those graphs.
 */
window.MSF_DIAGRAMS = {
  "dipole-arm": {
    title: "AR4 + permanent-magnet drive of a confined helical UMR",
    graph: [
      "flowchart LR",
      "    subgraph MIME[\"MIME node graph (ar4_helical_drive)\"]",
      "        direction LR",
      "        Arm[RobotArmNode<br/>arm] -->|end_effector_pose_world| Motor[MotorNode<br/>motor]",
      "        Motor -->|rotor_pose_world| ExtMag[PermanentMagnetNode<br/>ext_magnet]",
      "        ExtMag -->|field_vector<br/>field_gradient| MagResp[PermanentMagnetResponseNode<br/>magnet]",
      "        Body[RigidBodyNode<br/>body] -->|orientation| MagResp",
      "        Body -->|position| ExtMag",
      "        MagResp -->|magnetic_force<br/>magnetic_torque| Body",
      "        Gravity[GravityNode<br/>gravity] -->|gravity_force| Body",
      "        Body -->|position / vel / ω| MLP[MLPResistanceNode<br/>mlp_drag]",
      "        MLP -->|resistance_matrix| Lub[LubricationCorrectionNode<br/>lub]",
      "        Body -->|position / vel / ω| Lub",
      "        Lub -->|drag_force<br/>drag_torque| Body",
      "    end",
      "    U1[\"external: commanded_velocity\"] -.-> Motor",
      "    U2[\"external: joint_torques\"] -.-> Arm"
    ].join("\n")
  },
  "dejongh-mime": {
    title: "de Jongh 2025 helical-UMR replication (confined Stokes drive)",
    graph: [
      "flowchart LR",
      "    subgraph MIME[\"MIME node graph (dejongh_confined)\"]",
      "        direction LR",
      "        Field[ExternalMagneticFieldNode<br/>field] -->|field_vector<br/>field_gradient| MagResp[PermanentMagnetResponseNode<br/>magnet]",
      "        Body[RigidBodyNode<br/>body] -->|orientation| MagResp",
      "        MagResp -->|magnetic_force<br/>magnetic_torque| Body",
      "        Gravity[GravityNode<br/>gravity] -->|gravity_force| Body",
      "        Body -->|position / vel / ω| MLP[MLPResistanceNode<br/>mlp_drag]",
      "        MLP -->|resistance_matrix| Lub[LubricationCorrectionNode<br/>lub]",
      "        Body -->|position / vel / ω| Lub",
      "        Lub -->|drag_force<br/>drag_torque| Body",
      "    end",
      "    U1[\"external: frequency_hz\"] -.-> Field",
      "    U2[\"external: field_strength_mt\"] -.-> Field"
    ].join("\n")
  }
};

/**
 * msf.mermaidZoom(container)
 *
 * Attaches wheel-zoom + pointer-drag + double-click-reset to the first <svg>
 * inside `container`. Call this after mermaid has rendered the SVG.
 *
 * No D3 required — pure pointer-event math.
 */
window.msf = window.msf || {};
window.msf.mermaidZoom = function mermaidZoom(container) {
  // Wait a tick so the SVG is in the DOM after mermaid.run() resolves.
  requestAnimationFrame(function poll() {
    var svg = container && container.querySelector('svg');
    if (!svg) { requestAnimationFrame(poll); return; }

    /* ── state ─────────────────────────────────────────────── */
    var scale = 1, tx = 0, ty = 0;
    var dragging = false, startX = 0, startY = 0, startTx = 0, startTy = 0;
    var SCALE_MIN = 0.2, SCALE_MAX = 8, ZOOM_FACTOR = 0.0015;

    /* ── helpers ────────────────────────────────────────────── */
    function applyTransform() {
      svg.style.transform = 'translate(' + tx + 'px,' + ty + 'px) scale(' + scale + ')';
      svg.style.transformOrigin = '0 0';
      svg.style.willChange = 'transform';
    }

    function reset() {
      scale = 1; tx = 0; ty = 0;
      applyTransform();
    }

    /* ── wheel zoom (centred on cursor) ─────────────────────── */
    container.addEventListener('wheel', function(e) {
      e.preventDefault();
      var rect = svg.getBoundingClientRect();
      var ox = e.clientX - rect.left;
      var oy = e.clientY - rect.top;
      var delta = -e.deltaY * ZOOM_FACTOR;
      var newScale = Math.max(SCALE_MIN, Math.min(SCALE_MAX, scale * (1 + delta)));
      var ratio = newScale / scale;
      tx = ox - ratio * (ox - tx);
      ty = oy - ratio * (oy - ty);
      scale = newScale;
      applyTransform();
    }, { passive: false });

    /* ── pointer drag ───────────────────────────────────────── */
    container.addEventListener('pointerdown', function(e) {
      if (e.button !== 0) return;
      dragging = true;
      startX = e.clientX; startY = e.clientY;
      startTx = tx; startTy = ty;
      container.setPointerCapture(e.pointerId);
      svg.style.cursor = 'grabbing';
    });

    container.addEventListener('pointermove', function(e) {
      if (!dragging) return;
      tx = startTx + (e.clientX - startX);
      ty = startTy + (e.clientY - startY);
      applyTransform();
    });

    var endDrag = function() { dragging = false; svg.style.cursor = 'grab'; };
    container.addEventListener('pointerup',     endDrag);
    container.addEventListener('pointercancel', endDrag);

    /* ── double-click reset ─────────────────────────────────── */
    container.addEventListener('dblclick', reset);

    /* ── initial cursor + hint ──────────────────────────────── */
    svg.style.cursor = 'grab';
    container.title   = 'Scroll to zoom · Drag to pan · Double-click to reset';

    applyTransform();
  });
};
