# Divergences

Every intentional deviation from Wharfkit, one line of reason each. Anything not listed here is a bug in the port.

## Fixed by BLUEPRINT.md section 2

| Wharfkit | Dwarfkit | Why |
|---|---|---|
| `throw` | `Result<T>` return values, no exceptions in the public API | Unreal and godot-cpp build without exceptions by default |
| `async` / `Promise` | Blocking functions; caller runs them on a worker thread | Engines own scheduling; a C++ async layer would match neither engine |
| `Cancelable<T>` | `CancelToken` argument | Same semantics without a promise type |
| Decorators (`@Struct.type`, `@Struct.field`) | `DK_STRUCT` / `DK_FIELDS` macros plus compile-time reflection | No decorators in C++ |
| Plain JS objects | `dk::json` (nlohmann `ordered_json`) | Needs an untyped value type; insertion order kept for stringify parity |
| `fetch` | `FetchProvider` interface, `CurlFetchProvider` default | Engines supply their own HTTP |
| WebSocket (buoy) | `WebSocketProvider` interface, libcurl default | Same reason |
| `BrowserLocalStorage` | `FileSessionStorage`, `MemorySessionStorage` | No browser |
| Class instances passed by reference | `std::shared_ptr` for kits, plugins, providers, UI; value types for chain types | C++ ownership |
| String literals for names | `"eosio.token"_n` also accepted | One-liner ergonomics, matches CDT |
| pako (zlib) | miniz | Compressed ESR bytes may differ; parity is asserted on decoded payloads, not compressed URIs |
| `web-renderer` | Not ported | Engines render their own UI against the `UserInterface` contract |
| Scatter, Wombat, TokenPocket, MetaMask wallet plugins, `protocol-scatter` | Not ported | Browser `window.*` injection protocols; impossible without a browser |
| `WalletPluginCloudWallet` popup + `postMessage` | Protocol layer plus `WebViewBridge` interface; adapters supply the web view | No popups in a game engine |

## Found while porting

(append here, newest last)

| Wharfkit | Dwarfkit | Why |
|---|---|---|
| TS type guards (`Bytes.isBytes`, `isInstanceOf`, `arrayEquals`) | Dropped | C++ overload resolution and `operator==` replace runtime type sniffing |
| `Base58.DecodingError` subclass with `code`/`info` fields | `Error{kind: Invalid}` with `details["code"]` = "E_CHECKSUM"/"E_INVALID" plus the info pairs (byte arrays as hex strings) | Single error type across the library |
| `Bytes` shares the underlying buffer between instances | Value semantics, always copies | C++ vectors; upstream even warns about the sharing |
