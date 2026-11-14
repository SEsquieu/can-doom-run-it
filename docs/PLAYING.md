# Play DOOM I/O

Download the Windows development artifact from the Phase 2 PR's successful
Actions run. Extract **the entire archive** into a writable folder and launch
`can-doom-run-it.exe`. Keep the DLL, PK3s, Freedoom IWAD, soundfonts, FM banks,
and notices beside it. No Python, console, driver, IWAD selection, or network
connection is needed at runtime. Linux builds launch `build/linux/can-doom-run-it`.
These are unsigned development builds, not installers.

You spawn in **HELIX**, an original compact techbase. WASD moves, mouse looks,
left mouse shoots, and **E** uses an instrument. The entry hints explain the
stations; the right-hand monitor bank displays native adapter values when
approached. Output starts enabled into the built-in diagnostic endpoint.

| Station | Interaction | Generic output |
| --- | --- | --- |
| Amber pulse panel, far left | Shoot or Use | One trigger |
| Two amber momentary panels | Shoot or Use; repeat to maintain | Press, held lease, release after 12 ticks |
| Green latch, north wall | Use | Persistent toggle |
| Analog panel, west wall | Hold Use and look up/down; release to center | Scalar 0–1, neutral 0.5 |
| Recessed cyan floor | Walk across it; leave to center | Vector2 −1…1 on each axis |
| Three-position panel, northeast | Use repeatedly | Selector 0, 1, 2 |
| Endpoint station, southeast | Use | Select diagnostic or keyboard profile; never grants input permission |

Panels change visually and switches click. The gun has replenished ammo; there
are no enemies to interfere with operating the controls. The actors and map IDs
are reusable in other environments; their control IDs contain no endpoint keys.

## Control another application

**F6** opens the bundled **Input Lab / Sketchpad**, a separate native desktop
application that accepts ordinary keyboard messages. It selects the compatible
`keyboard.lab` profile. **F7** explicitly arms software input; F7 again releases
it. Discovery and profile selection never arm it. Keep DOOM focused while drawing;
on two monitors, place the sketchpad on the other display. On one monitor,
Alt-Tab to inspect it after operating controls; switching focus disarms DOOM.
The in-game diagnostic monitor remains visible without opening another window.

- Use the latch to lower/raise the pen (Shift).
- Walk the XY surface to move the pen (arrow keys).
- Hold the analog control to change movement speed (W/S).
- Shoot momentary A to stamp a marker (Space), B to brake (B).
- Use the selector to choose one of three inks (1/2/3).
- Shoot pulse to export a standard SVG drawing (Enter).

The sketchpad shows its export path. Windows defaults to
`%LOCALAPPDATA%/can-doom-run-it/sketch.svg`; Linux uses
`$XDG_DATA_HOME/can-doom-run-it/sketch.svg`, falling back to
`~/can-doom-run-it/sketch.svg`. Open that file in a browser or drawing editor.
A new export replaces the previous drawing. The canvas holds at most 4096 stroke
segments and 1024 markers. Restart the sketchpad for a new canvas. It also works
with a normal keyboard and has no knowledge of GZDoom, control IDs, or UDP.

This profile deliberately targets the shipped compatible application. On Windows
it uses directed Win32 window keyboard messages, not global SendInput, Raw Input,
or virtual HID. On Linux it requires X11/XWayland and uses directed XCB events;
native Wayland clients are unsupported. Games that require Raw Input, privileged
windows, arbitrary desktop programs, and virtual gamepads need additional endpoint
implementations and pairing UI. There is no silent driver installation.

Focus loss, menus, pause, output disablement, map change, endpoint loss, or shutdown
release held keys and revoke authority. A 350 ms worker watchdog releases keys
if the engine stops refreshing. Reconnection never rearms automatically. These
software permissions do not grant physical-device authority; hardware is disabled.
Demo playback and all multiplayer modes cannot publish external actions.

If no desktop endpoint is available, HELIX and the native diagnostic endpoint
continue working. F6/F7 status is shown in the lower overlay. The standard engine
console remains available for developers (`cdr_io_status`, `cdr_io_enabled`).
Explicit `-iwad` / `-file` launches preserve normal GZDoom behavior rather than
forcing the bundled room. Existing personal key bindings can override defaults.

See [VALIDATION.md](VALIDATION.md) for what was actually exercised, including
Windows limitations. Screenshot review and automated interactions do not replace
hands-on usability testing.
