# Can DOOM Run It? — DOOM I/O

A standalone GZDoom derivative turning playable spaces into software and hardware
control surfaces. The native runtime is endpoint-agnostic; no device protocol
belongs in a virtual instrument.

The Phase 2 prototype launches into **HELIX**, an original free techbase control
room with shootable buttons, toggles, an analog panel, an XY floor and a selector.
Its built-in diagnostic endpoint is ready immediately. Open the separate native
sketchpad with F6, authorize it with F7, and draw/export SVG through the same
instruments using routed OS keyboard input. No driver or terminal is required.

- [Play the prototype and understand endpoint limits](docs/PLAYING.md)
- [Build on Linux or Windows](docs/BUILDING.md), or open `.devcontainer/`
- [Native architecture and extension boundaries](docs/ARCHITECTURE.md)
- [Typed control protocol and developer diagnostics](docs/CONTROL_OUTPUT.md)
- [Free assets and redistribution](docs/ASSETS.md)
- [Upstream provenance and merge updates](docs/UPSTREAM.md)
- [Contributor conventions](AGENTS.md)
- [Actual validation and environment limits](docs/VALIDATION.md)

Based on official [GZDoom](https://github.com/ZDoom/gzdoom) `g4.14.2`.
The full upstream ancestry and original README commit are preserved. See the
[upstream README](docs/upstream/README.md) and original license files. Development
artifacts include verified Freedoom 0.13.0 and notices; commercial DOOM data is
never committed or packaged. Physical actuation is disabled in this milestone.
