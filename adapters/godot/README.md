# Dwarfkit Godot GDExtension

Thin adapter over the dwarfkit static library (BLUEPRINT.md 8.2): RefCounted
wrappers, one json <-> Variant conversion point, signal-based async, and a
DkUserInterface base class scripts extend. No kit logic lives here.

## Building

1. Check out godot-cpp (4.3+) into `godot-cpp/`.
2. Build the dwarfkit static library with curl OFF and copy every archive it
   produces into `lib/<platform>/`. dwarfkit links its dependencies
   privately and a static archive never bundles them, so the extension links
   five archives: `dwarfkit`, `libsecp256k1` (named `secp256k1` outside the
   MSVC generator), `secp256k1_precomputed`, `dk_trezor_crypto` and
   `dk_zlib`. The SConstruct also links the OS CSPRNG (`bcrypt` on Windows).

```
cmake -S <dwarfkit repo> -B build-godot -DDK_WITH_CURL=OFF -DDK_BUILD_TESTS=OFF -DDK_BUILD_TOOLS=OFF -DDK_BUILD_EXAMPLES=OFF
cmake --build build-godot --config Release --target dwarfkit
find build-godot -name "*.lib" -exec cp {} lib/windows/ \;   # or *.a into lib/<platform>/
```

3. `scons platform=<platform>` (set `DWARFKIT_ROOT` if the repo is elsewhere).
   The shared library lands in `demo/bin/`.

   On Windows the C runtime must match between godot-cpp and the dwarfkit
   library. The SConstruct defaults `use_static_cpp=no` (the dynamic /MD
   runtime, which is what the cmake command above produces). To ship a
   static runtime instead, build dwarfkit with
   `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` and run
   `scons platform=windows use_static_cpp=yes`.

## Pieces

- `DkSessionKit` (configure/set_ui/login/restore/logout/cancel/is_busy) with
  signals `login_completed(session)`, `restore_completed(session)`,
  `error(message)`. `set_ui` may be called before or after `configure`;
  `cancel` interrupts whatever is waiting on the wallet and leaves the kit
  usable; `login` and `restore` refuse to overlap (`is_busy`).
- `DkSession` (get_chain_id/get_actor/get_permission, transact) with signals
  `transact_completed(result)`, `error(message)`.
- `DkUserInterface`: extend in GDScript and override the `_prompt(args)`,
  `_status(message)`, `_error(message)` virtuals. Calls arrive on the main
  thread via call_deferred; the kit worker waits until each has run, with a
  bound and the kit's cancel token, so a freed UI never pins a worker.
- Providers: `HTTPClient` and `WebSocketPeer` polled from the worker thread
  with 30 s deadlines, a 64 MB response cap and a 16 MB message cap; storage
  under `user://dwarfkit/`.
- json <-> Variant: a uint64 above `INT64_MAX` (Godot's int) arrives as a
  String rather than an int, and goes back as a JSON string.

## Demo

`demo/login_transfer.tscn` runs `login_transfer.gd`: it configures Jungle 4,
starts an Anchor login (the QR `esr:` payload arrives in `_prompt`), then
broadcasts a 0.0001 EOS transfer and prints the transaction id.

## Lifetime and threading

Kit calls run on detached worker threads and results come back as signals;
never call kit methods from the main thread yourself. A worker never holds a
reference to its wrapper (it addresses the object by id, so a wrapper freed
by the script cannot be destroyed on its own worker and is never joined), it
shares ownership of the kit, and every wait it can sit in observes the kit's
cancel token, so `cancel`, reconfiguring and freeing the kit unwind workers
within a poll interval rather than freeing memory underneath them.
