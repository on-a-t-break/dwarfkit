# Dwarfkit Unreal Plugin

Thin adapter over the dwarfkit static library (BLUEPRINT.md 8.1): the four
interfaces implemented on Unreal subsystems, a GameInstance subsystem that
owns the `SessionKit`, and Blueprint async nodes. No kit logic lives here.

## Building the library

The plugin links prebuilt dwarfkit static libraries. Build them once per
platform with curl OFF (the adapter supplies FHttp/FWebSockets transports)
and the engine's CRT:

```
cmake -S <dwarfkit repo> -B build-ue -DDK_WITH_CURL=OFF -DDK_BUILD_TESTS=OFF -DDK_BUILD_TOOLS=OFF -DDK_BUILD_EXAMPLES=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build build-ue --config Release --target dwarfkit
```

Copy into `Source/ThirdParty/DwarfkitLib/`:

- `lib/<Platform>/`: every archive the build produced. dwarfkit links its
  dependencies privately and a static archive never bundles them, so there
  are five: `dwarfkit`, `libsecp256k1` (named `secp256k1` outside the MSVC
  generator), `secp256k1_precomputed`, `dk_trezor_crypto`, `dk_zlib`
  (`find build-ue -name "*.lib"` on Windows, `"*.a"` elsewhere). The
  Build.cs adds `bcrypt` on Windows for the OS CSPRNG.
- `include/`: the repo's `include/dwarfkit` tree, plus
  `third_party/expected/include/tl` as `include/tl` and
  `third_party/nlohmann/include/nlohmann` as `include/nlohmann` (the public
  headers include `<tl/expected.hpp>` and `<nlohmann/json.hpp>`). A verbatim
  copy of `third_party/` under `include/third_party/` also resolves.

Build a Release library for Development and Shipping targets: a Debug
library carries the debug CRT and iterator-debugging settings and will not
link against a non-Debug engine build. The module enables exceptions to match
the library (header-only code shared across the boundary must be compiled
the same way on both sides). No other flags are needed: the headers compile
under both MSVC preprocessors and avoid the engine's macro names. If your own
code calls `IdentityProof::verify` directly, wrap the call in
`#pragma push_macro("verify")` / `#undef verify` / `#pragma pop_macro("verify")`,
because CoreMinimal.h defines a macro of that name.

## Pieces

- `FDkUnrealFetchProvider` - FHttpModule request; completion is delivered on
  the HTTP thread, so the kit worker's wait never depends on the game thread
  ticking. 30 s timeout, 64 MB response cap.
- `FDkUnrealWebSocketProvider` - FWebSocketsModule socket created, connected
  and closed on the game thread; messages are reassembled from raw frames
  into a queue, and `receive` blocks the worker with the requested timeout
  and cancel token. 16 MB message cap.
- `FDkUnrealStorage` - session storage under `<ProjectSaved>/Dwarfkit/`
  through the engine's file layer.
- `FDkUnrealUserInterface` - forwards prompts/status to the Blueprint
  `UDwarfkitUI` assigned on the subsystem, on the game thread. A `qr` or
  `link` element's data is the `esr:` payload itself; render it with any QR
  widget.
- `UDwarfkitSubsystem` - owns the kit; `Configure(AppName, ChainId, Url)`
  wires WalletPluginAnchor over the Unreal websocket transport. `SetUI` may
  be called before or after `Configure`. `Cancel` interrupts whatever is
  waiting on the wallet and leaves the kit usable.
- Async nodes: `Login`, `Restore`, `Transact` (single action as JSON). They
  register with the game instance for the duration of the call, so an Anchor
  login that waits for a scan is not collected meanwhile.

## Lifetime and threading

Kit calls block and run on background threads; never call one on the game
thread (the fetch provider refuses with an error). Workers share ownership
of the kit and address UObjects weakly, and every wait they can sit in
observes the subsystem's cancel token, so a PIE stop, level travel or
`Cancel` unwinds them within a poll interval rather than freeing memory
underneath them.

## Sample map

Drop `ADwarfkitSampleActor` into an empty level and assign a `UDwarfkitUI`
to the subsystem. On BeginPlay it configures the subsystem for Jungle 4,
starts an Anchor login (the QR element's data is the `esr:` payload to
render), then broadcasts a 0.0001 EOS transfer and logs the transaction id.
