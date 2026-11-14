# Native control-output proof of concept

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

Example schema:

```json
{"schema":1,"sequence":1,"type":"trigger","control":"player.use","tick":70,"player":0}
```

`schema` versions the wire contract. `sequence` starts at 1 for the process's
first accepted event and advances even when a send fails. `tick` is elapsed
simulation time in the current map (35 ticks/second), not wall time, and resets
on a new map. `player` is the local player index. There is no session identifier
or acknowledgement; consumers must not deduplicate across process restarts by
sequence alone. Schema 1 currently has one primitive and one control identifier.

## Behavior and boundaries

- The hook is in `P_CheckUse`, directly in native gameplay code. The bus and
  adapter live in `src/cdr/` and do not depend on the engine's script VM.
- The bus serializes into a fixed 256-byte stack buffer. The adapter sends one
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
