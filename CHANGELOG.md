# Changelog

## 1.0.0 (2026-09-07)

First release.

- Ports every Wharfkit package that applies outside a browser: antelope, signing-request, abicache, common, protocol-esr (with buoy and sealed messages), session and its plugin hooks, wallet plugins (private key, Anchor, cleos, Cloud Wallet, TackleBox), contract, account, resources, msigs, actionstream, atomicassets, and the `dkgen` contract generator.
- Byte parity with Wharfkit's recorded fixtures: 443 test cases and 3037 assertions, green in Debug and Release with zero compiler warnings.
- Static library with `find_package(Dwarfkit)` support and a documented five-archive link set for engine build systems; the headers compile under MSVC's legacy preprocessor.
- Unreal Engine 5.4+ plugin and Godot 4.3+ GDExtension adapters. The Godot build is verified against godot-cpp 4.3 on Windows; the Unreal plugin is reviewed and compiles inside a UE project.
- Security hardening: bounded decoders (ABI depth, inflate output, response and message sizes), saturating numeric conversions, zeroised secrets, a randomized secp256k1 context, and 0600 session files.
- Project marks in `assets/`; the TackleBox mark ships as `WalletPluginTackleBox`'s logo.
