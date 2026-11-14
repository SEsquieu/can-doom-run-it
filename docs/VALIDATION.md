# Bootstrap validation — 2026-11-14

## Foundation milestone

Verified on Debian 13 x86-64, GCC 14.2, CMake 3.31.10, Ninja, with a two-CPU /
8 GiB container allocation. The imported baseline is GZDoom g4.14.2, commit
`99aa489d09015a95bb78df2b30ede29f328cc874`.

- Git ancestry checks passed for both the original project commit
  `3900c73359fb119206e96ffb7a806f79eb8e510a` and the upstream baseline. The
  baseline's 22,158 ancestor commits are retained by the import merge.
- `python3 scripts/build-zmusic.py --jobs 1` built and installed pinned ZMusic
  from a fresh build directory, independently of the initial dependency build.
- `cmake --preset linux` and `cmake --build --preset linux` completed the full
  Release engine, including Vulkan, OpenGL, GLES, software renderer, and VM/JIT.
- `LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3
  scripts/smoke-engine.py --iwad <freedoom2.wad>` passed: MAP01 loaded, simulation
  advanced, and the engine exited successfully. Evidence: `build/smoke/engine.log`
  and `build/smoke/console.log` (generated, untracked). The fixture uses actual
  gameplay startup, not `-norun`, plus explicit background/presentation settings.
- `python3 scripts/stage-dev.py linux` assembled the engine, PK3s, soundfont,
  FM banks, ZMusic libraries, and notices without an IWAD. The artifact is for
  development, not a finished installer.
- The pinned Ubuntu 24.04 development image built. A container run as its
  `ubuntu` user verified CMake 3.31.10, Ninja 1.13.0, SDL2, GLib, ALSA, and
  Freedoom availability. A complete engine build inside that image has not yet
  been run; the full native build above ran on the Debian host.
- Python scripts compiled, JSON presets parsed, and `git diff --check` passed.

## Environment fixes and limits

The selected environment had no sudo/package-install helper and incomplete
development headers. Dependencies were downloaded with APT signature/hash
verification from its configured Debian snapshot and extracted under
`/workspace/tooling/sysroot`, with CMake/pkg-config directed there. The extracted
SDL multiarch config header needed a local include-path bridge. System files
and HOME were not modified. Normal hosts/CI install development packages using
the build guide instead. Optional GTK dialogs were disabled in this host build.

Two engine build/runtime fixes are included: executable name is a CMake STRING
(FILEPATH converted the custom name into a source-tree absolute path), and
uninstalled builds retain CMake dependency RPATHs while supporting adjacent
libraries through `$ORIGIN`. Unix path handling now honors absolute XDG config
and cache directories, fixing startup with a read-only home directory.

Docker's standard socket was inaccessible; the managed rootless socket worked.
The environment's HTTPS proxy CA was supplied only as a temporary BuildKit secret
for pip. No proxy certificate, private credential, or local dependency prefix is
committed. The image's project Dockerfile retains standard TLS validation.

The development IWAD was Debian's Freedoom 0.13.0-2 `freedoom2.wad`, SHA-256
`92e73a81f75a03e29984e293da040928ddafd1cbad26a629489073a46e475347`.
No commercial data or downloaded IWAD is tracked.

Windows has a VS2022 x64 CI build definition; local Windows execution is not
available in this Linux environment. See the hosted evidence reconciliation below.
No physical-device testing, signed installer, bundled free IWAD/playground, or
zero-configuration device discovery is claimed by this foundation milestone.

## Native control-output milestone

The complete engine rebuilt successfully with `src/cdr/` linked into the native
application and a small rising-edge hook in `P_CheckUse`. Temporary debugging
instrumentation was removed before the final verification runs.

- The native code and tests compiled independently with GCC's `-std=c++17
  -Wall -Wextra -Wpedantic -Werror` and passed a real loopback UDP round trip.
- `ctest --test-dir build/linux -C Release --output-on-failure`: **1/1 passed**.
  The checks cover serialization, numeric bounds, invalid identifiers, missing
  sinks, local send failures, sequence gaps/counters, and actual UDP delivery.
- Live engine smoke, including a recorded demo: **two events emitted, two JSON
  datagrams observed** by a separate process. Holding Use for 12 ticks generated
  one event, followed by a second after release/repress. Observed ticks were
  **70 and 92**, sequences **1 and 2**, schema **1**, control **player.use**.
  Evidence: `build/smoke/live/events.jsonl` and the adjacent engine/console logs.
- The standalone `scripts/listen-control.py --count 2 --timeout 30` receiver
  independently captured the same two records from the final engine without
  debugging instrumentation. Evidence: `build/smoke/standalone-observer.jsonl`.
- Output-disabled smoke: **0 emitted / 0 observed**, normal map entry and exit.
- Receiver-absent smoke: **2 submitted**, normal map entry and exit with no
  observer, no retries, no blocking waits, and no crash.
- Playback of the recorded interaction: **0 emitted / 0 observed**, verifying
  suppression of replayed Use actions while output remains enabled.

The fixture starts from a seeded, isolated configuration each time. Its timed
command alias is loaded from INI before startup exec parsing; defining and using
a new alias in one startup script is not valid in this engine. No preexisting
user configuration or cached alias is required. Logs are retained by mode under
ignored `build/smoke/`. See `docs/CONTROL_OUTPUT.md` for repeatable commands.

The Linux/Windows workflow now builds the engine and runs native tests; Linux
also drives the positive, disabled, receiver-absent, and replay gameplay cases.
At the time of bootstrap validation, remote run listings returned no runs and
Actions settings inspection returned 403. That observation is superseded by the
hosted evidence below; Windows interactive execution remains unverified.

## Phase 2, stage 0: hosted evidence reconciliation

Inspected run [43538971197](https://github.com/SEsquieu/can-doom-run-it/actions/runs/43538971197)
and its uploaded `test-logs-linux` artifact (14091121986), plus job logs.
The run tested **b2f20409e**, not the later native-output commit b3f711895.

- Windows job **132274769584 succeeded**: ZMusic, complete engine build,
  development artifact staging and upload. This verifies Windows compilation
  and packaging at that commit, not interactive Windows runtime or Phase 2 code.
- Linux job **132274769165 compiled successfully**. `console.log` shows working
  llvmpipe/GLES initialization and `MAP01 - Hydroelectric Plant`. It also reports
  **`Unknown command "cdr_smoke"`** before gameplay. The uploaded `startup.cfg`
  defines and invokes the alias in the same startup exec file. GZDoom defers
  alias registration until after that invocation; the unknown command never
  schedules the timed quit. The 120-second timeout was an orchestration failure,
  not a compilation, rendering, map-loading, or insufficient-timeout problem.
- b3f711895 already moved the alias into the INI loaded before exec parsing.
  Phase 2 retains that fix and adds explicit begin/completion markers, immediate
  failure on the known missing-alias diagnostic, and process-group cleanup.
  The gameplay test remains enabled with its original timeout.
- The hosted version string was `<unknown version>` because origin does not
  carry upstream tags. `git describe --always` now provides commit provenance
  in fresh clones without requiring a mass import of upstream tags.

Downloaded evidence is retained untracked under `build/ci-evidence/`. No hosted
run results are inferred from local tests, and no repository settings changed.

The stage-0 correction is now verified by hosted run
[43542988421](https://github.com/SEsquieu/can-doom-run-it/actions/runs/43542988421)
at **e30ac4439**: Linux and Windows jobs both succeeded, including native tests,
all Linux gameplay smoke cases, development staging, and artifact uploads.
Run 43542277800 at the same commit also succeeded. This resolves the initial
Linux timeout. It does not imply that later, unpushed Phase 2 changes were tested.

## Phase 2 typed-model checkpoint

The existing Bus now supports all six requested primitives, bounded typed
metadata, schema 2 sessions/ordering, explicit transitions and transactional
snapshots, stale leases and lifecycle neutralization. The original player Use
hook and diagnostic receiver remain, updated intentionally for schema 2.

Local checks: native serializer/UDP tests and typed-model tests pass (2/2 CTest),
including all primitives, ranges, hold/release deduplication, stale values,
initial snapshots/resync, session distinction, adapter absence, permission
boundaries, focus/disable/map/shutdown cleanup, and capacity limits. Native code
also compiled with GCC `-Wall -Wextra -Wpedantic -Werror`. Python protocol-consumer
unit tests pass. The complete engine builds and the real player Use smoke still
produces exactly two schema 2 events with the same session and increasing
sequence/tick values. The script API is registered using GZDoom's native struct
bindings, with no arbitrary I/O or permission-grant methods exposed to maps.
