# Key Concepts

## Three-Layer USD Architecture

MICROBOTICA uses a three-layer USD composition stack:

1. **Base Layer** — the original scene file (immutable after load)
2. **Override Layer** — user parameter overrides via scripting
3. **Results Layer** — simulation output (never persisted)

## Compute Backends

A compute backend provides physics simulation and rendering.
MICROBOTICA ships with a local backend for development; cloud
backends ({term}`SkyPilot`) will be available in Phase 2.

## Scripting Console

The embedded Python console provides access to the `microrobotica`
module for scene inspection and parameter manipulation.
