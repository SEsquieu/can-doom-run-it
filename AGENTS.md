# Can DOOM Run It? contributor and agent guide

This is a standalone GZDoom derivative turning spatial gameplay interactions
into hardware/software control. Keep it a native application. Preserve the
upstream engine, history, licenses, rendering, and gameplay compatibility.

## Work conventions

- Read `docs/UPSTREAM.md`, `docs/BUILDING.md`, and `docs/ARCHITECTURE.md` before
  changing engine integration. Use small, reviewable commits for completed work.
- Place project code in `src/cdr/`, support scripts in `scripts/`, and project
  docs in `docs/`. Keep upstream hooks minimal; do not broadly reformat engine code.
- Native code is C++17. Follow surrounding formatting in upstream files; use
  explicit types, RAII, bounded data, and clear ownership in new code. Project
  source uses SPDX GPL-3.0-or-later headers. Never remove upstream notices.
- Never commit IWADs, commercial assets, secrets, dependency downloads, binaries,
  generated PK3s, build outputs, or personal configuration. Use ignored `build/`.
- Preserve upstream ancestry; use merges for updates, no force-pushing shared
  history. Record baseline changes and cherry-pick attribution.

## Build and verify

Linux: install the packages in `docs/BUILDING.md`, then run
`python3 scripts/build-zmusic.py`, `python3 scripts/fetch-free-assets.py`, `cmake --preset linux`, and
`cmake --build --preset linux`. Windows: the same dependency script with Python 3 plus `python scripts/build-openal.py`,
then `python scripts/fetch-free-assets.py`, `cmake --preset windows` and `cmake --build --preset windows` in VS2022 x64.
CI must build both complete engines. See the build guide for executable locations.

For native I/O changes, test serialization, ordering, filtering, sink failures,
and bounded behavior. Run the engine integration smoke test with free assets:
a gameplay interaction must produce a parseable event observed by a separate
process. A console-only test does not prove the gameplay path. Avoid tests that
merely mirror implementation. Record actual commands/results and any untested
platforms in `docs/VALIDATION.md`. Never claim Windows or physical-device testing
from a Linux-only run. Manually check first-run and normal play for UI changes.

## Architecture and product principles

- Native bus owns structured, versioned control events. Gameplay produces generic
  primitives; endpoint adapters translate them. Device protocol logic stays out
  of actors, maps, rendering, and ZScript.
- Prove one complete path before adding plugin registries, broker services,
  routing graphs, or protocol frameworks. Never block a game tick on device I/O.
- Simulation/replay/network activity must not accidentally duplicate real outputs.
  Start with the local player only; define authority before multiplayer support.
- No endpoint configured means a working built-in observable demo, not a setup
  wizard. Hardware may require explicit pairing; offline launch still works.
- Endpoint loss must not crash gameplay. Future stateful outputs need safe
  disconnect/stop behavior, bounded queues, and visible connection status.
- Long-term: download, launch, enter the free built-in playground, control
  something immediately. Distinguish that target from bootstrap functionality.

## Phase 2 control and content conventions

- Preserve schema 2 session/sequence and snapshot transactions. Never turn a
  trigger snapshot into an action or silently change a primitive's contract.
- Scripts may define/publish generic controls and read diagnostics. Never add
  arbitrary I/O, device protocols or permission grants to the VM API.
- Every asynchronous adapter needs a bounded mailbox, visible failures, an
  out-of-band cleanup path and stale-heartbeat neutralization. New physical
  capabilities require independent authority design and verified safe endpoints.
- Keep map `user_control` IDs stable; do not identify instruments by spawn order
  or endpoint keys. Keep endpoint/profile selection outside map geometry.
- Original HELIX source is in `content/`. Rebuild PK3s through CMake. Only use
  `scripts/build-content.py --write-map-source` when deliberately replacing the
  editable UDMF geometry. Sector fields are `heightfloor`/`heightceiling`.
- Run Python tests, CTest and `smoke-engine.py --room --controls --external` for
  instrument or routing changes. Inspect real engine screenshots for visual
  changes. The separate keyboard endpoint test must assert observable behavior,
  neutral state and SVG output; API success alone is insufficient.
- Stage bundles only after fetching verified free assets. Preserve Freedoom,
  ZMusic and engine notices, source revision, native companion and runtime files.
  Windows compilation and window-message tests do not prove interactive GZDoom
  rendering or arbitrary third-party application compatibility.
