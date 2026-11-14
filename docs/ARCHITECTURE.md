# Native control runtime

Can DOOM Run It? is a standalone GZDoom derivative. The native C++ runtime owns
control validation, state, authority, adapter delivery, and lifecycle cleanup.
Maps and ZScript are an instrument authoring surface, not the external I/O runtime.
Upstream engine ancestry and gameplay compatibility are retained.

```text
player shooting / Use / position
  -> reusable ZScript instrument, stable map user_control ID
  -> narrow DoomIO native struct binding (src/cdr/game_io.cpp)
  -> typed native Bus, session / sequence / state / authority
       -> native diagnostic adapter -> in-world monitor overlay
       -> nonblocking UDP loopback -> optional developer observer
       -> capability-gated keyboard adapter -> bounded worker queue
            -> Win32 / XCB endpoint -> separate sketchpad -> exported SVG
```

## Model and authority

The bus supports trigger, button, toggle, scalar, Vector2, and selector, with up
to 32 bounded definitions. Metadata includes ranges, neutral values, units,
selector count, update behavior and optional stale leases. Events describe
transitions; transactional snapshots describe current state. Process sessions,
monotonic sequences and periodic/requested snapshots support resynchronization.
See [CONTROL_OUTPUT.md](CONTROL_OUTPUT.md) for the wire protocol.

Gameplay definitions contain no endpoint names or OS keys. A local live pawn is
the only allowed publisher. Demo playback and multiplayer (including `-host 1`)
are suppressed in native code, independently of map behavior. Menu, pause and
window focus also gate the bus. Native map-load and shutdown hooks neutralize
outputs; the map hook clears map definitions so other maps can reuse IDs with
new metadata. Restored HELIX saves re-register definitions before publishing.
Software input is a separate, nonpersistent capability granted only by native
F7/user action, never by the script API, profile selection or device discovery.
Physical capability requests are rejected.

## Instrument API

GZDoom's existing `struct DoomIO native` and `DEFINE_ACTION_FUNCTION(_DoomIO,…)`
bindings avoid a new VM subsystem or native Actor hierarchy. The API is declared
in `wadsrc/static/zscript/cdr_api.zs`. `Define` registers typed metadata;
`Publish(localPawn, id, x, y)` submits validated values. Readonly value, diagnostic,
profile and status accessors drive visual feedback. `SelectProfile` can choose
an existing compatible profile but cannot grant it authority. There is no file,
network, OS input, arbitrary protocol, or permission-grant API available to maps.

`content/zscript/instruments.zs` implements reusable shootable momentary controls,
Use toggles, a spring-centered look-controlled analog panel, a position-derived
XY pad, and selectors. UDMF `user_control` strings provide stable IDs independent
of spawn order. Roles on diagnostic props affect presentation only. Control
semantics remain native even if an actor fails to send release: leased controls
expire and lifecycle resets take precedence. Buttons discard pending holds on
reset; analog instruments drop their operator if native publication is rejected.

## Adapter boundary and concurrency

`Adapter` has an ID, explicit required capabilities, readiness, bounded `Send`,
`Tick`, and `Reset`. Statically registered built-ins share one Bus without changes
to gameplay. Message references are valid only during Send. External adapters
must copy required data into owned bounded storage and never wait on the game
thread. Diagnostics are in-process; UDP uses one bounded nonblocking datagram.

The keyboard adapter uses a 64-entry single-producer/single-consumer ring and a
worker owning the endpoint. Resets increment an out-of-band epoch, so queue
saturation cannot hide release. Old-epoch actions are discarded. Overflow,
endpoint failure and missed heartbeat fault the adapter and require a new user
arm. The worker releases held keys on reset and deterministic thread shutdown.
A 350 ms wall-clock watchdog protects against a stopped simulation. Endpoint
lookup and Win32/XCB calls run on the worker, never inside a simulation tick.

`KeyboardProfileCompatible` describes the required primitive/ID set and a
three-state selector. `KeyboardAdapter` is the mapping profile; `KeyEndpoint`
translates generic key transitions to platform APIs. Current mapping applies
continuous values around declared neutral/range with a dead zone; its threshold
output is digital keyboard input, not analog HID. Snapshots reconcile held keys
and selector state without replaying trigger actions. Focus loss releases owned
keys and restores the selector neutral index. See [PLAYING.md](PLAYING.md).

New built-in adapters implement the same boundary and expose their capabilities
and profiles. For future third-party adapters, use the versioned loopback stream
as an out-of-process extension seam now; an external observer can subscribe and
resync without relinking the engine. This stream grants no physical authority.
A future loadable native ABI must version copied C-compatible records, negotiate
capabilities and lifecycle, and remain optional; no unstable C++ plugin ABI is
promised. MIDI, OSC, serial, microcontrollers, network APIs, robotics, gamepads
and live video remain extension directions, not implemented features. There is
no broker, database, distributed registry or general dependency injection layer.

## First run and content

When no explicit IWAD/file/demo was supplied, native startup selects the adjacent
verified Freedoom IWAD and original `doom-io.pk3`, then loads DIO01 directly.
Default control bindings and a native diagnostic endpoint work offline without a
terminal. Missing optional endpoints never prevent gameplay. Native F6 opens a
separate useful keyboard-controlled sketchpad and F7 authorizes the selected
profile. Endpoint selection can also happen at an in-world station, without
editing the map. No global keyboard injection or hardware discovery is implicit.

The original text UDMF room and procedural artwork generator are source; build
outputs and downloaded assets stay in ignored build directories. The standard
library Python content builder produces deterministic PK3 metadata and original
PNG textures, signs and stateful sprites. It runs during the CMake build;
packaging includes the free IWAD, notices, engine resources, ZMusic and companion
application. The package is a development ZIP, not a signed installer. Its exact
verification and remaining platform limits are recorded in VALIDATION.md.
