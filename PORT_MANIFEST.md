# Port manifest

One row per upstream repo (all of github.com/wharfkit at 2026-08-26, plus greymass/buoy-client). Classification: **port** (becomes a Dwarfkit module), **reference** (consulted, not ported), **skip** (not applicable). Status: `-` (not started), `wip`, `done vX` (ported at upstream version X). Commits are the shallow-clone HEADs in `reference/`; refresh a row whenever a module is re-synced against upstream.

34 repos classified port, 24 reference, 23 skip.

| Repo | Version | Commit | Classification | Status | Notes |
|---|---|---|---|---|---|
| abicache | 1.2.4 | b8fd381 | port | - | Phase 2 |
| account | 1.4.1 | fd370fe | port | - | Phase 4 |
| account-creation-plugin-anchor | 1.4.0 | d5138c2 | port | - | Phase 3; the blueprint's "AccountCreationPluginGreymass" (Greymass account creation service) |
| account-creation-plugin-jungle4 | 1.2.0 | b9f2fab | port | - | Phase 3 |
| account-creation-plugin-metamask | 1.3.0 | 3b00177 | skip | - | needs the MetaMask browser extension |
| account-creation-plugin-template | 1.0.0 | 653b653 | reference | - | template; informs examples/ |
| actionstream | 0.4.0 | a7c8608 | port | - | Phase 4 extras; Roborovski action stream client over WebSocketProvider |
| antelope | 1.2.0 | bb3c9fb | port | - | Phase 1 core. Also contains a p2p/ module not in the blueprint checklist; decide at end of Phase 1 (needs a socket interface) |
| antelope-rs | - | 8ff21c7 | reference | - | Rust port; cross-check when the TS is ambiguous |
| api-client-template | 0.0.0 | 367bbc9 | reference | - | template |
| assets | - | 52cc2be | reference | - | brand/media assets, no code |
| atomicassets | 1.3.1 | 6b3a08e | port | - | Phase 4 extras |
| bundle | 0.1.2 | 2e5e6fb | reference | - | npm meta-bundle, no logic |
| buoy-client | 1.0.4 | 7dc9c71 | port | - | Phase 2 (greymass org) |
| chain-logo | - | 47e69e4 | reference | - | logo CDN assets |
| chains | - | EMPTY | skip | - | empty repository |
| cli | 2.11.0 | 0a2848c | port | - | Phase 6, as dkgen (generate subcommand only) |
| common | 1.5.0 | 31f7106 | port | - | Phase 2 |
| conformance | 0.1.0 | 39834b9 | reference | - | float-op oracle contract on Jungle 4 (conform.gm); optional DK_LIVE_TESTS consumer for float parity |
| console-renderer | 0.1.1 | b7146d3 | reference | - | informs ConsoleUserInterface |
| contract | 1.3.0 | 2c98897 | port | - | Phase 4 |
| data-type-template | 0.0.0 | e9c5f35 | reference | - | template |
| discussions | - | d84533e | skip | - | GitHub discussions repo, no code |
| docs | - | 8d9c893 | reference | - | documentation site |
| example-nodejs | 1.0.0 | 4560e97 | reference | - | informs examples/ |
| example-p2pclient | 1.0.0 | 4c24cba | reference | - | pairs with antelope p2p/ decision |
| example-vite-react-ts | 0.0.0 | 6854e08 | reference | - | web example |
| example-vite-svelte-ts | 0.0.0 | e7fb1b5 | reference | - | web example |
| example-vite-sveltekit-ts | 0.0.1 | ee1af02 | reference | - | web example |
| example-vite-vue-ts | 0.0.0 | 4a38f53 | reference | - | web example |
| hyperion | 1.0.5 | be41ae9 | port | - | Phase 4 extras; Hyperion API client |
| issues | - | 4823d3e | skip | - | meta repo, no code |
| js | 4.0.0-rc2 | 639cf3a | reference | - | upcoming all-in-one v4 bundle; watch for API changes |
| login-plugin-template | 1.0.0 | 0610c4b | reference | - | template; informs examples/ |
| mock-data | 1.3.1 | 9cf701d | port | - | Phase 1; MockFetchProvider (makeMockFetch key scheme) + recorded fixtures |
| msigs | 0.3.1 | c8ccca7 | port | - | Phase 4 extras; Roborovski msigs API client. NOTE: the blueprint's "msig proposal transact plugin" does not exist as an org repo |
| protocol-esr | 1.6.1 | 1b61ed2 | port | - | Phase 2 |
| protocol-scatter | 1.4.0 | 846bd39 | skip | - | browser window injection |
| resources | 1.6.0 | 0af2e3c | port | - | Phase 4 |
| roborovski | 1.1.0 | 2025112 | port | - | Phase 4 extras; Roborovski API client |
| sealed-messages | 1.2.0 | 8fc32b0 | port | - | Phase 4 extras; Shamir secret sharing + sealed messages (the blueprint's "Shamir package") |
| session | 1.6.1 | 467f049 | port | - | Phase 3 |
| signing-request | 3.4.0 | 18a13f7 | port | - | Phase 2 |
| skill | - | f87fb83 | skip | - | Claude Code skill for wharfkit development, no library code |
| starter | - | 253797a | reference | - | starter kit |
| svelte-components | 0.7.0 | e338750 | skip | - | web UI components |
| token | 1.2.0 | 14c6779 | port | - | Phase 4 |
| transact-plugin-autocorrect | 1.4.1 | 6d4db2f | port | - | Phase 3 |
| transact-plugin-cosigner | 1.1.0 | 7125c53 | port | - | Phase 3 |
| transact-plugin-explorerlink | 1.0.1 | 6837e47 | port | - | Phase 3 |
| transact-plugin-finality-callback | 1.0.0 | c1ba8ce | port | - | Phase 3 |
| transact-plugin-finality-checker | 1.0.0 | 9a70f93 | port | - | Phase 3 |
| transact-plugin-mock | 1.1.0 | 83e9be1 | port | - | Phase 3 |
| transact-plugin-resource-provider | 1.2.0 | 92d8637 | port | - | Phase 3 |
| transact-plugin-template | 1.0.0 | 215ee56 | reference | - | template; ported as examples/ |
| tutorial-client | 0.0.0 | 26fcb26 | reference | - | tutorial app |
| tutorial-todo-contract | - | 17f1ce6 | reference | - | sample contract; dkgen golden-test candidate |
| ui-plugin-template | 0.1.0 | 0906118 | reference | - | template |
| wallet-plugin-anchor | 1.7.3 | c460bc2 | port | - | Phase 5 |
| wallet-plugin-cleos | 1.2.0 | 3c86de8 | port | - | Phase 5, dev only |
| wallet-plugin-cloudwallet | 1.6.5 | 5017944 | port | - | Phase 5; protocol layer + WebViewBridge interface |
| wallet-plugin-gatewallet | 1.1.0 | d59fa2a | skip | - | Scatter browser protocol |
| wallet-plugin-imtoken | 1.1.0 | 51f1d3d | skip | - | Scatter browser protocol |
| wallet-plugin-ledger | 0.1.0 | a468070 | skip | - | WebUSB/WebHID transport; revisit only if native HID is demanded |
| wallet-plugin-metamask | 1.2.1 | e0d37ea | skip | - | MetaMask browser extension |
| wallet-plugin-mimic | 1.1.0 | 81ac40d | skip | - | unmodified template fork (class still WalletPluginTEMPLATE) |
| wallet-plugin-mock | 1.1.0 | ac5fb00 | port | - | Phase 3 |
| wallet-plugin-paycash | 1.1.0 | e6c2ada | port | - | Phase 5; ESR-based like Anchor |
| wallet-plugin-privatekey | 1.1.0 | f611111 | port | - | Phase 3, first wallet |
| wallet-plugin-scatter | 1.5.1 | 2d7848d | skip | - | browser |
| wallet-plugin-template | 1.1.0 | 7dac61f | reference | - | template; informs examples/ |
| wallet-plugin-tokenpocket | 1.6.3 | 500ddb9 | skip | - | browser |
| wallet-plugin-web-authenticator | 0.5.1 | 2a17393 | skip | - | browser popup WebAuthn UI |
| wallet-plugin-wombat | 1.5.1 | 96e27be | skip | - | browser |
| web-renderer | 1.4.3 | d6464fb | reference | - | prompt semantics for UserInterface |
| web-ui | 0.4.0 | a9b4c5d | reference | - | web |
| webauthn | 1.3.0 | bb2bdde | reference | - | browser-side WA key/signature creation; informs WA verification in antelope |
| website | 0.0.1 | 1293679 | skip | - | marketing site |
| wharfkit-godot | - | ee5e242 | reference | - | existing GDScript integration; informs the Godot adapter API |
| wharfkit-godot-wallet-plugin-anchor | - | bd4d07d | reference | - | same |
| wharfkit-rs | - | 22f2f27 | reference | - | Rust port; cross-check |

## Third-party (build dependencies, not ports)

See third_party/README.md for versions, sources and local patches.
