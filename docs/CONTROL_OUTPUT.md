# Native control runtime and protocol

In a live, single-player game, each press of **Use** emits a native `trigger`
event to **UDP 127.0.0.1:19793**. Holding Use emits once until release/repress.
Normal door/switch/conversation handling continues. `player.use` means the
player attempted Use; it does not claim a door or switch successfully activated.

## Try it

Build the application using [BUILDING.md](BUILDING.md). In one terminal:

```sh
python3 scripts/listen-control.py
```

In another terminal, launch with free assets:

```sh
build/linux/can-doom-run-it -iwad /path/to/freedoom2.wad -warp 1
```

Press Use (normally Space or E, depending on bindings) in the game. The receiver
prints JSON immediately. Windows uses `py -3 scripts/listen-control.py` and
`build/windows/Release/can-doom-run-it.exe`; place the installed ZMusic DLL beside
that executable first. No WAD script, broker, plugin registration, or endpoint
configuration is required. Run only one receiver on the fixed port at a time.

Schema 2 event example (schema 1 was the bootstrap wire contract):

```json
{"schema":2,"session":"ca05219f0b0b383d","sequence":17,"tick":70,"kind":"event","snapshot":0,"control":"player.use","type":"trigger","player":0,"phase":"trigger","value":[1,0],"revision":1}
```

`schema` versions the wire contract. Schema 2 deliberately changes the bootstrap
contract to distinguish events from state and process launches. `session` is a
per-process opaque identifier. `sequence` advances for every message, including
snapshots and failed submissions; it is monotonic within one session. `tick` is
the engine simulation clock (35 ticks/second), not wall time. `revision` counts
changes to that control. `player` identifies the local operator.

## Typed definitions and states

Every control has a stable lowercase identifier, label, primitive, units,
minimum/maximum, two-component neutral value, update behavior, selector count,
and optional stale timeout in simulation ticks. Registration is capped at 32
controls. Redefinition with conflicting metadata is rejected. Values are finite;
scalar/vector values clamp to their declared ranges, booleans require 0 or 1,
and selectors require an integral index in range.

| Primitive | Transitions | Snapshot value / safety behavior |
| --- | --- | --- |
| Trigger | One `trigger` event per action, value `[1,0]` | Neutral `[0,0]`; revision records pulse count. Snapshots never fire actions. |
| Button | `press`, held lease refresh, `release` | Current 0/1 state; refreshes do not duplicate press events. |
| Toggle | `change` | Latched 0/1 state until another change or safety reset. |
| Scalar | `change` | Current ranged X and zero Y; declared neutral on expiry/reset. |
| Vector2 | `change` | Current ranged X/Y; both axes return to neutral. |
| Selector | `change` | Current zero-based discrete index. |

`definition` messages carry metadata. `event` messages carry transitions.
Snapshots are transactions: `snapshot_begin`, a definition and `state` for each
control, then `snapshot_end`, all sharing one nonzero `snapshot` identifier.
Clients commit snapshots only when complete and ordered; ordinary events use
`snapshot:0`. Trigger states remain neutral and never replay one-shot actions.

A new adapter receives a snapshot on attachment/authorization. UDP consumers
bind **127.0.0.1:19793** and send the exact ASCII request **`DIO/2 SNAPSHOT`** to
**127.0.0.1:19794**. The native socket accepts at most four requests per frame,
rate-limits snapshots, and accepts no control writes or authority commands.
Periodic snapshots every 35 ticks repair missed state. After a sequence gap or
session change, discard partial snapshots and request a fresh one. The Python
receiver maintains an atomic state cache using this contract.

Focus loss, output disablement, map cleanup, and shutdown neutralize stateful
controls and revoke software-keyboard authority. Continuous instruments and
momentary buttons refresh their declared leases; stale values neutralize even
if a script fails to release them. Physical-device grants are rejected in this
milestone. Diagnostic/loopback adapters never confer OS-input permissions.

## Behavior and boundaries

- The hook is in `P_CheckUse`, directly in native gameplay code. The bus and
  adapter live in `src/cdr/` and do not depend on the engine's script VM.
- The bus serializes into a fixed 1536-byte stack buffer. The adapter sends one
  bounded datagram using a nonblocking socket and has no retry queue or waits.
  It only addresses IPv4 loopback; nothing listens inside the engine.
- No receiver is required to launch or play. UDP submission is best effort;
  successful `sendto` means the OS accepted bytes, not that anyone received them.
- `cdr_io_status` in the game console shows availability and emitted/submitted/
  dropped counts. Drops count local serialization/send failures, not undetectable
  UDP loss. `cdr_io_enabled false` disables output; the default is enabled.
- Remote players, network games, and demo playback do not emit. These policies
  must be revisited explicitly before multiplayer/device authority is enabled.

This proves an observable engine-to-software path. Physical hardware adapters,
discovery, a built-in visual endpoint, and the packaged first-run playground
remain future work. Future physical outputs need explicit pairing/authority and
state/disconnect handling; a generic UDP listener is not a hardware adapter.

## Automated verification

```sh
ctest --test-dir build/linux -C Release --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3 scripts/smoke-engine.py \
  --controls --iwad /path/to/freedoom2.wad
```

The native test checks exact serialization, numeric bounds, sequence gaps after
failure, counters, invalid controls, missing sinks, and a real UDP round trip on
an ephemeral port. These checks stay active in Release builds and run in Linux
and Windows CI. The engine test drives two Use presses separated by a hold and
release, checks exactly two JSON datagrams outside the engine, verifies sequence
and tick ordering, and saves the evidence in `build/smoke/live/events.jsonl`.

Repeat the engine test with `--disable-output` to expect zero emissions and
`--no-listener` to verify normal exit without a receiver. Use `--record-demo
build/smoke/live/control-demo.lmp` with a live test, then `--play-demo` with that
path to check replay suppression. Logs are separated by test mode under
`build/smoke/`; none are source-controlled.
