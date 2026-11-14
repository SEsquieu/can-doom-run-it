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
  libx11-dev libxcursor-dev libasound2-dev
python3 scripts/build-zmusic.py
cmake --preset linux
cmake --build --preset linux
```

CMake >=3.21 and GCC with C++17 are required. CI uses CMake 3.31.10; the preset
sets the compatibility policy floor for vendored projects with older CMake
minimums. ZMusic 1.1.14 is built from commit
`89f3d65734470fb7ec0c1e69f73a0cfcc88ed557`, under `build/deps/`.
Optional GTK dialogs/OpenAL runtime libraries can be installed for a full desktop
experience. Vulkan headers and several codecs are vendored upstream.

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
cmake --preset windows
cmake --build --preset windows
```

The executable is `build/windows/Release/can-doom-run-it.exe`. ZMusic is built
from the same source pin; GZDoom's vendored Windows libraries remain upstream.
Supply the installed ZMusic DLL beside the executable when launching/packaging.
CI uploads development artifacts with notices, not an end-user installer.

## Assets and tests

Freedoom Phase 2 is the default development IWAD; see [ASSETS.md](ASSETS.md).
The engine and PK3s alone do not include a complete game. Never use commercial
DOOM data in CI or commit downloaded IWADs. Linux interactive launch:

```sh
build/linux/can-doom-run-it -iwad /path/to/freedoom2.wad -window -nosound
```

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
