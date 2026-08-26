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
| `Int8..UInt64` wrapper classes with C++11-emulating operators | Native `int8_t..uint64_t`; `Int128`/`UInt128`/`VarInt`/`VarUInt` remain real types; `intToJSON` keeps the >32-bit-becomes-string rule | The wrappers reimplement C++ in JS; the port runs on the original |
| `Float32`/`Float64` wrapper classes | Native `float`/`double`; toString/toJSON rules live in the serializer builtins | BLUEPRINT.md 5.6 type map |
| `Asset.value` getter throws when units exceed 53 bits | `Asset::value()` returns the full double; the strict 53-bit error stays on `Symbol::convertUnits` | C++ doubles have no 53-bit API cliff; parity kept where the upstream test asserts |
| `toFixed` rounds decimal ties toward positive infinity | `Symbol::convertFloat` uses printf rounding (ties to even) | Differs only when value*10^precision is exactly representable at a .5 tie |
| `TimePoint.equals(badString)` throws | Returns false | equals returning Result would break comparison ergonomics |
| `_n` name literal accepts what `Name.from` accepts (silent mangling) | `_n`, `_asset`, `_symbol` are consteval and reject invalid literals at compile time | Compile-time validation is free and matches CDT |
| elliptic for K1 and R1 curves | libsecp256k1 for K1 (custom nonce reproduces elliptic's HMAC-DRBG with pers=[attempt]; byte-parity proven), trezor-crypto nist256p1 for R1 | No elliptic in C++; K1 parity is asserted, R1 tests are round-trip only |
| `PrivateKey` mutable `.data` reassignment (a test sets `k.data = Bytes.random(31)`) | Not supported; construct via `PrivateKey::make` which enforces the 32-byte invariant | That test asserts a throw C++ gives at construction |
