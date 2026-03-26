# Runtime Validation Deferred

Components that have been compile-verified under ASan+UBSan but lack runtime test coverage. Each entry describes what scenario would validate the component, what the trigger for validation will be, and which sanitizer run documented the gap.

This document consolidates information previously scattered across individual MBCA-SAN records in `docs/validation/sanitizer_runs.yaml`.

---

## SshTunnel (MBCA-COMP-051)

| Field | Value |
|---|---|
| **Sanitizer record** | MBCA-SAN-003 |
| **Compile-verified in** | Docker (GCC 13.3.0, ASan+UBSan, Qt 6.4.2) |
| **What is not tested** | Opening an actual SSH tunnel, monitoring `QProcess::finished()` on tunnel death, port forwarding lifecycle |
| **Runtime validation scenario** | Open an SSH tunnel to a live SkyPilot cloud instance, forward ZMQ ports, receive ResultFrames through the tunnel, then kill the remote instance and verify `tunnelDied` signal fires |
| **Validation trigger** | First cloud deployment against a live SkyPilot cluster |
| **Why not testable now** | Docker test environment has no SSH target. Testing requires a real remote host with SSH access and SkyPilot-managed keys |
| **Risk if broken** | Cloud mode connection fails silently or tunnel death goes undetected, causing stale frame display (related: MBCA-ANO-003) |

---

## ExperimentRunner (MBCA-COMP-061)

| Field | Value |
|---|---|
| **Sanitizer record** | MBCA-SAN-004 |
| **Compile-verified in** | Docker (GCC 13.3.0, ASan+UBSan, Qt 6.4.2) |
| **What is not tested** | Spawning `python3 -m mime.runner`, monitoring process lifecycle, crash detection via `processCrashed` signal |
| **Runtime validation scenario** | Launch a MIME runner subprocess for an experiment directory, verify ZMQ frames arrive via MimePhysicsProcess, then kill the subprocess and verify `processCrashed` fires within 10s (heartbeat timeout) |
| **Validation trigger** | First live MIME connection (local subprocess mode) |
| **Why not testable now** | Docker test environment does not have the MIME Python package installed. Testing requires a working MIME installation with JAX |
| **Risk if broken** | Local experiment launch fails, or crashed MIME process goes undetected |

---

## CloudJobLauncher (MBCA-COMP-062)

| Field | Value |
|---|---|
| **Sanitizer record** | MBCA-SAN-004 |
| **Compile-verified in** | Docker (GCC 13.3.0, ASan+UBSan, Qt 6.4.2) |
| **What is not tested** | Invoking `sky jobs launch` via QProcess, parsing launch output, cancelling a running job |
| **Runtime validation scenario** | Launch a SkyPilot job from MICROBOTICA using a real `jobs/*.yaml` config, verify the job appears in `sky jobs queue`, connect to it via ConnectionManager, then cancel it and verify cleanup |
| **Validation trigger** | First cloud deployment with SkyPilot installed |
| **Why not testable now** | Docker test environment has no SkyPilot CLI installation or cloud credentials |
| **Risk if broken** | Cloud experiment launch fails, or cost safety nets (autostop, budget limits) are not applied correctly |

---

## ConnectionDialog (connection_dialog.h/.cpp)

Intentionally unregistered — no MBCA-COMP ID. This is a pure UI helper function (a `QMessageBox` with custom buttons), not a component with physics, IPC, or data-integrity logic. It has no ComponentMeta, no preconditions/postconditions, and no verification benchmark. It is listed here only because it cannot be exercised headlessly.

| Field | Value |
|---|---|
| **Sanitizer record** | MBCA-SAN-004 |
| **Compile-verified in** | Docker (GCC 13.3.0, ASan+UBSan, Qt 6.4.2) |
| **What is not tested** | QMessageBox display, button click handling, QInputDialog for manual endpoint entry |
| **Runtime validation scenario** | Open an experiment with no MIME running, verify the three-button dialog appears, click each button and verify the correct `ConnectionChoice` is returned |
| **Validation trigger** | First interactive UI session with experiment loading |
| **Why not testable now** | Qt widget tests require a display server (`QApplication` with a real or virtual X11/Wayland display). Headless ASan runs cannot exercise widget event loops |
| **Risk if broken** | User cannot choose connection mode when no MIME process is auto-detected |

---

## Phase F UI Panels (MBCA-SAN-005)

All Phase F panels are Qt UI widgets that require a display server. Compile-verified under ASan+UBSan. Runtime validation deferred to first interactive UI session.

Intentionally unregistered — no MBCA-COMP IDs. These are pure UI panels with no physics, IPC, or data-integrity logic. The one testable component (AnsiParser) IS fully unit-tested under ASan (10 tests).

**Panels**: SimulationToolbar, MimeConsolePanel, RunConfigPanel, SkyPilotMonitorPanel, ProjectBrowserPanel, SettingsDialog, ThemeManager.

| Field | Value |
|---|---|
| **Sanitizer record** | MBCA-SAN-005 |
| **Compile-verified in** | Docker (GCC 13.3.0, ASan+UBSan, Qt 6.4.2, OpenUSD v24.08) |
| **What is not tested** | Widget creation, layout rendering, user interaction, signal/slot wiring between panels and core services |
| **Runtime validation scenario** | Launch MICROBOTICA with a loaded experiment, verify all panels render correctly, exercise each panel's controls, verify simulation toolbar enables/disables correctly, verify MIME console displays output, verify settings persist across restarts |
| **Validation trigger** | First interactive UI session |
| **Why not testable now** | Qt widget tests require a display server. Headless Docker ASan runs cannot exercise widget event loops |
| **Risk if broken** | Panels fail to render or interact, simulation controls unresponsive, settings not persisted |

---

## Validation checklist

When a component is validated at runtime, update this document by:
1. Adding a **Validated** row with the date and context
2. Removing the entry from the deferred list (or marking it as resolved)
3. If a defect is found during validation, file an MBCA-ANO entry in `docs/validation/known_anomalies.yaml`
