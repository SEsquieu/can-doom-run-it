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
`python3 scripts/build-zmusic.py`, `cmake --preset linux`, and
`cmake --build --preset linux`. Windows: the same dependency script with Python 3,
then `cmake --preset windows` and `cmake --build --preset windows` in VS2022 x64.
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
