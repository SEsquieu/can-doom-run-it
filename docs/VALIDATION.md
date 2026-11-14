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

## Phase 2 adapter checkpoint

Hosted run [43548328033](https://github.com/SEsquieu/can-doom-run-it/actions/runs/43548328033)
at **185de2be1** succeeded on Linux and Windows. In addition to both full engine
builds, CTest and preserved Linux gameplay tests, both runners launched a separate
native window, routed actual OS keyboard messages, observed movement/actions in
its report and verified held keys returned to zero. This is Windows window-message
runtime evidence, rather than an inference from compilation. It does not test the
later HELIX content or Windows GZDoom interactive graphics.

## Phase 2 playable local checkpoint

On the same Debian host, the full engine rebuilt with the typed native runtime,
VM bindings, keyboard adapter and first-run startup hook. Three native CTests
and eight Python tests pass. Native lifecycle tests exercise press/hold/release,
snapshot reconciliation without trigger replay, focus reset, a stalled heartbeat,
bounded queue overflow, endpoint failure and deterministic worker shutdown.
Content tests reject unplayable sector heights and missing stable instrument IDs.

Real GLES/software-rendered gameplay under Xvfb passed these additional cases:

- Bundled first-run path: no `-iwad`, `-file` or `-warp`; DIO01 loads and all eight
  definitions (including existing player.use) register. The test seeds isolated
  renderer/configuration settings and a timed exit; it is not a hands-on launch.
- Actual hitscan shots press pulse and both momentary buttons. Both buttons
  produce exactly one press and one release, separated by the held interval.
- Real Use activates latch and selector. Holding Use and looking up drives the
  analog scalar from 0.5 through intermediate values to 1; release restores 0.5.
- Walking/warping onto the XY region produces `[0.5,0.5]`; leaving restores `[0,0]`.
  Test positioning uses the engine's ordinary warp command; interactions use
  ordinary attack, Use, look and movement inputs, never direct native Publish.
- The gameplay-to-OS-to-sketchpad test produces markers and line strokes in an
  actual external SVG drawing. The independent process reports held=0, fires=1,
  pulses=2, brakes=1, mode=0, positive X/Y movement and nonempty strokes. The SVG
  is parsed independently for path and circle elements. Native diagnostic and
  loopback adapters simultaneously consume the same generic stream.
- Recording live room play, then replaying it: no external event publications.
  Output-disabled and one-player multiplayer-host cases likewise emit no events.
- Real engine screenshots reviewed for room geometry, signs, panel state and HUD.
  This caught incorrect UDMF height property names and bench-relative spawn Z;
  both were corrected. Graphical inspection verifies rendered content, not a claim
  of polished hands-on usability. The compact room requires user playtesting.
- Linux staging includes verified Freedoom, original content, separate native
  application, engine resources, ZMusic and component notices. No downloaded
  IWAD, generated PK3, screenshot, build directory or binary is source-controlled.

Generated evidence is under `build/smoke/`, `build/screenshots/` and
`build/keyboard-endpoint.{json,svg}`. Hosted CI repeats these checks and uploads
logs, drawings and screenshots; its exact final run is recorded after completion.
The original player Use positive/disabled/receiver-absent/replay cases remain in CI.

Windows interactive GZDoom rendering, hands-on room usability, audible feedback,
signed installation and arbitrary third-party target compatibility have not been
verified here. Windows classic window-message routing is tested in a real separate
process. Native Wayland, Raw Input/HID, hardware actuation, automatic permissions,
and general endpoint discovery are intentionally not implemented. The shipped
software endpoint needs no driver; all physical-device authority remains denied.

The complete playable checkpoint **4810dd855** is verified by hosted run
[43549371020](https://github.com/SEsquieu/can-doom-run-it/actions/runs/43549371020):
Linux and Windows both succeeded. Both built the full engine, tested lifecycle
and real OS keyboard routing, verified external SVG output, and staged first-run
bundles with legal assets. Linux additionally passed the preserved player Use
suite and every room live/external/record/replay/disabled/multiplayer/screenshot
case. Its Windows development artifact is
[can-doom-run-it-windows](https://github.com/SEsquieu/can-doom-run-it/actions/runs/43549371020/artifacts/14095880764).
This checkpoint is the first hosted playable build; later verification refinements
are recorded separately rather than attributed to this run.

Subsequent local verification also launches the sketchpad through the application's
native `cdr_open_lab` path, with no prestarted endpoint. The test changes actual X11
window focus while the toggle is held. Native `focus_lost` neutralization and
permission revocation occur before shutdown; the independent endpoint reports
zero held keys and neutral selector. Snapshot-consumer tests additionally reject
mixed snapshot IDs and unknown-control state growth, and prevent trigger events
from becoming latched cache state. Linux child reaping is nonblocking on the
endpoint worker. These refinements retain the already successful gameplay path.

The Windows artifact archive was downloaded and its SHA-256 matched the hosted
digest (`abc2ee2f56732c1a084687144b8c5fc09cfbe46dd17f0b333e29627419beb0a2`
for the 4810 checkpoint). Inspection confirmed both executables, original room,
free IWAD, Freedoom notices and ZMusic DLLs. PE import inspection found only
bundled ZMusic plus Windows system DLLs; the native application and ZMusic use
the static MSVC runtime. This is packaging evidence, not Windows engine playtesting.

Additional true-color room screenshots were reviewed with `vid_rendermode=4`
on Mesa llvmpipe. A requested Vulkan startup fell back to GLES/OpenGL presentation
in this host, so Vulkan runtime validation is not claimed. The software-rendered
GLES fixture remains the primary repeatable CI gameplay path. Audio devices are
unavailable in this container; smoke tests use `-nosound`.

## Audio packaging and hosted observer race

Run 43550615167 at 20884d2a7 compiled both engines and passed Linux, but its
Windows external-window test reported a missing final pulse. The parallel PR
run 43550622296 passed. Investigation of the failed job (132311981861) showed
that a fixed 30 ms probe delay allowed shutdown to invalidate the queued pulse
before the desktop app processed it. The fixture now waits for independently
observed SVG output, maintains the heartbeat, then explicitly acknowledges the
probe before shutdown. Cleanup and the 350 ms runtime watchdog remain intact;
there is no blanket timeout increase or skipped assertion.

A Windows artifact audit also found missing dynamically loaded OpenAL. Packaging
now builds official OpenAL Soft 1.24.3 from its pinned source, includes the DLL
and all component notices, and tests actual nonzero loopback PCM. The same pinned
library built on Linux and rendered 4096 samples locally. This closes the runtime
packaging gap without claiming audible playback or a Windows graphical engine
test. Later hosted runs exercise the Windows DLL and corrected observer handshake.
