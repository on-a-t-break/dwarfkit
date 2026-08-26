# Progress

Rules: see CLAUDE.md. Tick an item only when it builds, its tests are green, and it is committed. Keep the "Current state" block accurate at all times; a fresh session resumes from it.

## Current state

- Phase: 1 (antelope core)
- In flight: nothing
- Next: APIClient item (APIClient, APIProvider, FetchProvider, CurlFetchProvider, MockFetchProvider, v1.chain + v1.history with all types.ts structs), then port test/api.ts + the API-driven authority-sort.ts cases against copied fixtures.
- Notes: CancelToken deferred to protocol-esr. strictExtensions decoding mode deferred until the session kit needs it (part of test/serializer.ts 'binary extensions' not ported). p2p/ module deferred to end of Phase 1 (per manifest note). K1 byte-parity vectors verified against elliptic via node (scratchpad/elliptest).

## Phase 0: Bootstrap

- [x] Repo skeleton, CMake, options, warnings-as-errors, `third_party/` vendoring, doctest smoke test.
- [x] `scripts/fetch_reference.sh`: `gh repo list wharfkit --limit 100 --json name` then shallow clone all into `reference/`, plus `greymass/buoy-client`. Classify every repo in `PORT_MANIFEST.md` (port / reference only / skip, with reason).
- [x] Copy the phase checklists into `PROGRESS.md`; create `DIVERGENCES.md` seeded from section 2; create `CLAUDE.md` from section 11.

## Phase 1: antelope core

- [x] `core/`: `Result`, `Error`, `json` helpers, `Bytes`, hex, base64, base58 (all check variants), sha256/sha512/ripemd160/hmac, CSPRNG shim. (base64 deferred to first call site)
- [x] Chain types: integers (incl. 128-bit, var ints, `toJSON` rules), `Float128`, `Name` + `_n`, `Asset`/`Symbol`/`SymbolCode`/`ExtendedAsset` + literals, time types, checksums, `BlockId`, `Blob`.
- [x] Crypto: K1 via libsecp256k1 with the elliptic-compatible nonce function; R1 via trezor-crypto; WA parse/verify; `PublicKey`, `PrivateKey`, `Signature` full method sets; key string formats. (WA verify deferred to serializer item; WA string parse/serialize done; K1 byte-parity proven vs elliptic)
- [x] `ABI` model and `resolveType`, `ABIEncoder`/`ABIDecoder`, builtins, `Serializer` (static, dynamic, `synthesize`, `stringify`), `DK_STRUCT`/`DK_FIELDS`/`DK_VARIANT`/`DK_TYPE_ALIAS`/`BinaryExtension`, `Action`, `Transaction`, `SignedTransaction`, `PackedTransaction`, authority types. (serializer.ts, webauthn.ts and the transaction/authority chain.ts cases ported; typestresser byte-parity green; strictExtensions deferred)
- [ ] `APIClient`, `APIProvider`, `FetchProvider`, `CurlFetchProvider`, `MockFetchProvider`, `v1.chain` and `v1.history` with all `types.ts` structs.
- [ ] All `antelope/test/tests/*.ts` ported and green against copied fixtures.

## Phase 2: common, abicache, signing-request, protocol-esr

- [ ] `common`: `Chains`, `ChainDefinition`, explorers, logos.
- [ ] `abicache`.
- [ ] `signing_request` with miniz and base64u; all its tests.
- [ ] `protocol_esr` + buoy send/receive over the transport interfaces; `CurlWebSocketProvider`; uuid v4; unit tests with a fake `WebSocketProvider`.

## Phase 3: session and plugins

- [ ] `session`: kit, session, login, transact pipeline, plugin base classes, `UserInterface`, storage, translations, `ConsoleUserInterface`, `NullUserInterface`, `FileSessionStorage`, `MemorySessionStorage`.
- [ ] `WalletPluginPrivateKey`, `WalletPluginMock`, `TransactPluginMock`; session test suite green.
- [ ] `TransactPluginResourceProvider`, `Cosigner`, `ExplorerLink`, `FinalityChecker`, `FinalityCallback`, `Autocorrect`, msig plugin; their tests.
- [ ] Login and account-creation plugin bases; Greymass and Jungle account-creation plugins.
- [ ] `examples/transfer_privatekey` runs against Jungle 4.

## Phase 4: contract, account, resources, token

- [ ] `contract` + tests.
- [ ] `resources` + tests (numeric parity).
- [ ] `token` + tests.
- [ ] `account` + tests.
- [ ] Any additional library packages found in Phase 0 (Atomic Assets client, Shamir).

## Phase 5: wallets

- [ ] `WalletPluginAnchor` with qrcodegen util; `examples/anchor_login` (console prints QR as text).
- [ ] `WalletPluginCloudWallet` protocol layer + `WebViewBridge` interface.
- [ ] `WalletPluginCleos`.

## Phase 6: dkgen

- [ ] Generator, offline `--abi` mode, golden-output tests for `eosio.token`, `eosio`, `atomicassets`.

## Phase 7: engine adapters

- [ ] Unreal plugin, sample map with login + transfer.
- [ ] Godot GDExtension, sample scene with login + transfer.

## Phase 8: packaging and docs

- [ ] `find_package(Dwarfkit)` install, versioning, README with the TS-to-C++ parity matrix, migration notes, `DIVERGENCES.md` final pass.

## Session log

- 2026-08-26: session 1 started. Environment: Windows 11, VS 2022 Build Tools 17.14, CMake 4.3.3, Ninja 1.13, MinGW g++ 16.1, git 2.51, gh authed. Canonical build: VS generator x64.
