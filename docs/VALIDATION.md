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
available in this Linux environment. CI results must be checked after pushing.
No physical-device testing, signed installer, bundled free IWAD/playground, or
zero-configuration device discovery is claimed by this foundation milestone.
