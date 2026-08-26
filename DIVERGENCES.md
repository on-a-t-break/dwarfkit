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
| `@Struct.type` decorators, `Struct.from(value)` | `DK_STRUCT`/`DK_FIELDS` on plain aggregates; construct with designated initializers or `dwarfkit::structFrom<T>(json)` | No decorators; a member `from` would collide with fields named `from` (transfer) and break aggregate init |
| `Struct`/`Variant` runtime classes built from `abiFields` arrays, `customTypes` decoding, encoder/decoder `metadata` | Not ported; the typed path is compile-time (`DK_` macros + `abi_traits`), the dynamic path is json + ABI | TS runtime class machinery has no C++ counterpart; revisit only if a ported call site needs it |
| Untyped JS-object encode (`Serializer.encode({object: {name: Name...}})` type inference) | Not ported; dynamic encode always takes a type name + ABI | json values carry no C++ type information |
| Fixed-size array (`int32[4]`) decode reads a varuint length prefix (upstream bug, encode writes none) | Decode honors the fixed size symmetrically | Upstream decode of a fixed array cannot round-trip its own encode |
| `strictExtensions` decode option with `abiDefault` synthesis | Deferred until the session kit needs it | Tracked in PROGRESS.md |
| Self-referential optional struct fields (`self?: Complex`) | `std::shared_ptr<T>` field maps to `T?` | `std::optional` requires a complete type |
| miniz (BLUEPRINT.md section 3) | Vendored zlib 1.3.1 | pako is byte-identical to zlib, keeping packed-transaction fixture hashes and ESR URIs byte-compatible; miniz streams differ |
| api `FetchProvider` class | `FetchAPIProvider` (wraps the transport `FetchProvider` interface) | Two types cannot share the name in one namespace; the transport interface keeps the blueprint-pinned name |
| `BoolType.from` passes any value through | `bool` fromJSON accepts booleans and 0/1 numbers, rejects the rest | nodeos emits 0/1 for some bool fields |
| Untyped API interfaces (`PushTransactionResponse`, `SendTransactionResponse`, `GetAbiResponse`, params bags) | Plain `json` | Upstream never runs them through the serializer either |
| `ChainDefinition.accountDataType` (a class reference) | Dropped; chain-specific account types are template parameters (`BasicAccountObject<TelosAccountVoterInfo>`) at call sites | C++ cannot store a type in a struct |
| Telos/WAX account objects re-declare `voter_info` on a subclass | `BasicAccountObject<VoterInfo>` template | Field re-declaration has no C++ equivalent |
| `ABICache.pending` promise map | A mutex; concurrent getAbi calls for one account serialize | BLUEPRINT.md 6.3; no promises to share |
| mock-data `makeMockFetch` records `{method, body}` without empty headers | `MockFetchProvider` omits the headers key when empty | Matches the recorded fixture hashes |
| `SigningRequestEncodingOptions.zlib` provider object (compression only when passed) | `bool zlib = true` (built-in zlib); a request decoded/created keeps its flag for later `encode()` | zlib is vendored anyway; pass `{.zlib = false}` for parity with TS call sites that omit the provider |
| `SigningRequest.create` / `identity` / `fromPayload` are async (`createSync` variants exist) | One blocking method each | BLUEPRINT.md 2: no promises |
| Placeholder resolution rebuilds typed action data via `Struct.from` walking decoded objects with `Name.isInstance` checks | JSON walk over the decoded action data replacing exact strings `............1` / `............2` | The decoded representation is json; bare strings equal to a placeholder name only occur as name values |
| `SigningRequest.data` is a `RequestData` struct instance whose `req` variant differs by version | `std::variant<RequestDataV2, RequestDataV3>` member | The two ABI layouts (IdentityV2/IdentityV3) need distinct C++ types |
| Checksum classes are final in dwarfkit up to Phase 1 | `final` removed from `Checksum160/256/512` | `ChainId extends Checksum256` upstream |
| `SigningRequest.getInfoKey(key)` returns string, `(key, type)` overloads infer typed decode | `getInfoKey(key)` string, `getInfoKey<T>(key)` typed, `getInfoKey(key, type)` dynamic json | C++ overload set replaces TS union-typed params |
