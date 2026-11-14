# Game assets and redistribution

The engine's GPL source license does not grant rights to commercial DOOM data.
Do not commit or redistribute DOOM/DOOM II IWADs, shareware data as a default
bundle, extracted textures/sounds, or third-party mods with unverified terms.

## Bundled development and packaging assets

**Freedoom Phase 2 0.13.0** (`freedoom2.wad`) provides a complete, freely
redistributable DOOM II-compatible game. Phase 1 (`freedoom1.wad`) is also
appropriate; FreeDM is intended for multiplayer and is not the first-run default.
Official project: https://freedoom.github.io/ and
https://github.com/freedoom/freedoom/releases/tag/v0.13.0.

Freedoom uses a permissive BSD-style license. Redistribution must preserve its
copyright, conditions, and disclaimer; do not imply endorsement by its authors.
For releases, ship the exact release's COPYING and credits with its IWAD,
record its source URL/version/SHA-256 in the packaging manifest, and keep engine,
Freedoom, and project-authored content attribution separate. The source repo
contains neither downloaded IWADs nor commercial assets. Development can use
the distro `freedoom` package. Release packaging must use a pinned, verified
official release, not an unversioned download or the developer's installed WAD.

## Other bundled material

Preserve GZDoom's `LICENSE`, `docs/licenses/`, source headers, and notices for
vendored libraries, `soundfont/`, `fm_banks/`, and generated PK3 inputs. An engine
binary distribution needs corresponding source under GPL-3.0-or-later and all
applicable component notices. Inventory all runtime dependencies before release;
the development CI artifact is not a completed redistributable product package.

The first-run development ZIP includes the official Freedoom Phase 2 0.13.0
release. `scripts/fetch-free-assets.py` pins the official URL and verifies archive
SHA-256 `3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59`
before extraction. Packaging preserves unmodified COPYING, CREDITS,
CREDITS-MUSIC, README, NEWS and a provenance manifest in `licenses/Freedoom`.
No download or external asset selection is necessary at runtime.

HELIX geometry, signs, procedural textures and instrument sprites are original
project work under GPL-3.0-or-later, authored as text UDMF/ZScript and generated
from `scripts/build-content.py`. The generator contains its own simple bitmap
letterforms; it uses no external font, texture pack, image service or commercial
game material. Freedoom supplies the legal player/weapon, sounds and music.
Generated PNG/WAD/PK3 files remain in ignored build directories. Content and its
source are included through the exact-revision source link in each bundle.
