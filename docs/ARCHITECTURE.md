# Native control foundation

Can DOOM Run It? is a standalone application built on GZDoom, with native I/O
as an engine subsystem. Maps and scripts can become an authoring surface; they
are not the runtime or the sole implementation of control behavior.

## First vertical slice

```text
local player's Use press (P_CheckUse, simulation tick)
    -> generic trigger: player.use
    -> native bus: version + session sequence + tick + local player
    -> built-in structured-output adapter
    -> independent process observes one JSON record
```

Only a rising edge emits; holding Use does not flood outputs. Existing use-line
behavior continues unchanged. The first implementation will be single-thread
owned on the simulation thread and target a local observable sink. It does not
attempt real hardware discovery. The proof must be tested through actual gameplay.

## Native I/O bus

The bus owns event validation, sequencing, and a transport-independent contract.
Events contain a schema version, primitive type, stable control identifier,
simulation tick, player identity, and payload. A session sequence distinguishes
multiple events in a tick; it restarts on process launch. Simulation ticks are
not wall-clock timestamps. Consumers must not assume cross-session exactly-once
delivery. Version incompatible schema changes explicitly.

Keep a narrow C++ API. Do not expose arbitrary sockets, files, or protocol
commands to untrusted mods as a shortcut. The initial sink is local observability.
Future network/device adapters must hand off to bounded worker queues, expose
drop/disconnect counters, and never wait for a remote endpoint on a tick.

## Generic control primitives

Start with **trigger** (an instantaneous action). Later primitives can include
**button** (press/release), **scalar** (a value with explicit units/range), and
**vector** (spatial value with coordinate frame and units). Add each only when
a concrete control path needs it. Stateful controls require initial state,
resynchronization, neutral values, and shutdown/disconnect handling. Do not model
hardware pin numbers or vendor-specific requests as gameplay primitives.

## Endpoint adapters

An adapter maps a generic control contract to an endpoint capability. The first
adapter needs no registry, routing graph, database, broker, or SDK. Later examples
could be a loopback software service, MIDI, serial, OSC, or a GPIO bridge. Choose
the next adapter based on a real device and test the entire round trip. Discovery
and pairing belong at the adapter boundary, separate from gameplay. Surface
availability, capability mismatch, and failures in application UI.

Local live gameplay is the authority for the bootstrap path. Demo playback,
remote players, dedicated servers, and duplicate simulation must not silently
activate external devices. Physical outputs need an explicit authority and
arming model before implementation.

## Zero-configuration first run (target experience)

1. A signed/versioned package includes the engine, free IWAD, original control
   playground, default bindings, licenses, and a built-in visual endpoint.
2. Launch creates project-specific user state and selects bundled content without
   an IWAD picker or dependency download. Offline launch works.
3. A room presents an obvious interaction. Press Use; a visible indicator changes
   immediately and the structured event is available in diagnostics.
4. Compatible software endpoints can advertise capabilities. Unavailable devices
   do not block the built-in demo. Hardware pairing/authorization is contextual,
   with clear status, rather than mandatory global configuration.

Bootstrap delivers the engine and the native observable event path first. The
bundled playground, visual endpoint, automatic content selection, installer,
and discovery UI are subsequent work, not claims about the initial build.
