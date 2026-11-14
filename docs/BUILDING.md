# Reproducible development builds

Use a full Git clone (not a source ZIP) so version generation and upstream
ancestry work. Source and dependency revisions are pinned; these are repeatable
build recipes, not a claim of byte-identical builds across toolchains.

The `.devcontainer/` definition provides Ubuntu 24.04, compiler/development
packages, free test assets, and pinned CMake/Ninja. Open the repository in a
Dev Containers-compatible editor, or follow the equivalent host steps below.
The container supports headless smoke tests; interactive graphics require a
host display setup. The base image is pinned by digest; distro package updates
are intentionally allowed. Dependency source revisions and build-tool versions
are fixed. Update the image digest deliberately when refreshing the environment.

## Linux (Debian 13 / Ubuntu 24.04, x86-64)

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends build-essential git cmake ninja-build \
  python3 pkg-config libsdl2-dev libvpx-dev libbz2-dev libgl-dev \
  libx11-dev libxcursor-dev libasound2-dev libxcb1-dev libopenal1 libxtst6
python3 scripts/build-zmusic.py
python3 scripts/fetch-free-assets.py
cmake --preset linux
cmake --build --preset linux
```

CMake >=3.21 and GCC with C++17 are required. CI uses CMake 3.31.10; the preset
sets the compatibility policy floor for vendored projects with older CMake
minimums. ZMusic 1.1.14 is built from commit
`89f3d65734470fb7ec0c1e69f73a0cfcc88ed557`, under `build/deps/`.
Optional GTK dialogs can be installed for desktop file selection. Linux audio
uses the system OpenAL runtime (`libopenal1`). Vulkan headers and several codecs are vendored upstream.

The executable is `build/linux/can-doom-run-it`; keep its generated PK3 files,
soundfonts, and fm_banks alongside the application when assembling a package.
Build-tree runtime discovery of ZMusic uses CMake's build RPATH; release bundles
must supply the dependency beside the executable and satisfy all runtime libraries.

Unix user state uses `$XDG_CONFIG_HOME/can-doom-run-it` (default
`~/.config/can-doom-run-it`) and `$XDG_CACHE_HOME/can-doom-run-it` (default
`~/.cache/can-doom-run-it`). Relative XDG overrides are ignored. The smoke test
isolates state under `build/smoke/` without overriding HOME. The application name,
configuration directory, and save signature are distinct from upstream GZDoom;
its engine/content compatibility version remains 4.14.2.

## Windows (Visual Studio 2022, x64)

Install Visual Studio's Desktop development with C++ workload, Windows SDK,
CMake >=3.21, Git, and Python 3. From a Developer PowerShell:

```powershell
py -3 scripts/build-zmusic.py
py -3 scripts/build-openal.py
py -3 scripts/fetch-free-assets.py
cmake --preset windows
cmake --build --preset windows
```

The executable is `build/windows/Release/can-doom-run-it.exe`. ZMusic is built
from the same source pin; GZDoom's vendored Windows libraries remain upstream.
The content build copies the installed ZMusic and OpenAL DLLs beside the
executable. OpenAL Soft 1.24.3 is built from pinned commit
`dc7d7054a5b4f3bec1dc23a42fd616a0847af948` with the static MSVC runtime; no
separate audio or VC runtime installation is needed for the Windows bundle.
CI stages the ZMusic DLL and uploads a complete development ZIP with verified
Freedoom, HELIX, the independent native sketchpad and notices. Extract everything
and launch the executable; no developer tools are needed to play. This is not a
signed end-user installer.

## Assets and tests

`python3 scripts/fetch-free-assets.py` verifies the pinned official Freedoom
0.13.0 ZIP by SHA-256 and stages only Phase 2 plus original notices under ignored
`build/assets/`. Do this before building; CMake's `cdr_content` target generates
HELIX and copies the verified IWAD beside the executable. No commercial assets
are used. A developer can omit this download for explicit `-iwad` testing, but
first-run packaging requires it. See [ASSETS.md](ASSETS.md).

```sh
build/linux/can-doom-run-it
```

This loads the bundled room automatically. Explicit `-iwad`, `-file`, or demo
arguments retain standard GZDoom selection. Original source is in `content/`:
edit `content/maps/DIO01.udmf` or the reusable ZScript actors, then rebuild.
`scripts/build-content.py` generates deterministic PK3/PNG/WAD resources using
only the Python standard library. Its optional `--write-map-source` deliberately
regenerates the text geometry; normal builds preserve hand edits.

Linux engine integration smoke (enters MAP01 under a virtual display and quits):

```sh
sudo apt-get install --no-install-recommends xvfb xauth freedoom
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 scripts/smoke-engine.py \
  --iwad /usr/share/games/doom/freedoom2.wad
```

Run native tests with `ctest --test-dir build/linux -C Release --output-on-failure`
(Windows uses `build/windows`). See [CONTROL_OUTPUT.md](CONTROL_OUTPUT.md) for
the native control demo and positive/negative gameplay tests. Build artifacts,
dependency checkouts, IWADs, and logs belong under ignored `build/`.
CI builds the complete engine on both target platforms and runs this smoke on Linux.
The headless test uses software rendering with GLES presentation, caps rendering,
and allows simulation without window focus. It does not validate every renderer.

For memory-constrained hosts reduce `cmake --build build/linux --parallel 2`.
Do not remove engine features merely to hide a missing local dependency.

## Playable room and external endpoint verification

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3 scripts/smoke-engine.py --room
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3 scripts/smoke-engine.py --room --controls --external
xvfb-run -a python3 scripts/test-keyboard-endpoint.py --preset linux
python3 scripts/stage-dev.py linux
```

Windows uses `python scripts/test-keyboard-endpoint.py --preset windows`,
`ctest --test-dir build/windows -C Release --output-on-failure` and
`python scripts/stage-dev.py windows`. The OS endpoint test launches a separate
native window, verifies actual keyboard-driven canvas movement, SVG export and
key release. It does not require an IWAD or GZDoom renderer. Linux room tests
exercise real hitscan, Use and positional instruments, then assert both native
protocol values and the external application's resulting SVG file. CI also
covers disabled output, demo playback and multiplayer suppression. Native
lifecycle tests cover focus, stale heartbeat, endpoint failure and queue overflow.

Stage outputs under `build/stage/<preset>` include the complete first-run bundle,
source revision and runtime notices. Linux bundles still require system SDL2,
X11/XCB, GL and audio libraries; use a compatible distro. Windows dependencies
are adjacent to the executable. The input adapter targets classic Win32 messages
or X11; native Wayland, Raw Input games and arbitrary-window pairing are not
supported. See [PLAYING.md](PLAYING.md) and [VALIDATION.md](VALIDATION.md).

The Windows workflow also tests the actual bundled audio DLL through OpenAL
Soft's loopback renderer and checks nonzero PCM from a generated tone, without
requiring speakers. `python scripts/test-audio-runtime.py --library
build/deps/openal/bin/OpenAL32.dll` repeats it. This validates library loading
and rendering, not physical audio playback. Linux audio hardware remains a host
runtime requirement. Linux Actions ZIP extraction may normalize executable
permissions; run `chmod +x can-doom-run-it doom-io-input-lab` for that development
artifact. Normal local CMake builds retain executable permissions.

`xvfb-run -a python3 scripts/test-first-run.py` performs a fresh **zero-argument**
launch, using actual X11 F6/F7/Use keys and normal console quit instead of seeded
aliases or renderer/configuration arguments. It requires `libxtst6`. CI runs it
in addition to the isolated, deterministic gameplay fixtures. The bundled
soundfont's output name matches the derivative's native default patch name.
