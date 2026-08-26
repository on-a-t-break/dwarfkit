# Dwarfkit

Native C++20 port of [Wharfkit](https://github.com/wharfkit) (the Greymass SDK suite for Antelope blockchains), built as a static library for C++ software, primarily Unreal Engine and Godot.

Same package boundaries, class names, method names, option shapes, hook names and error semantics as Wharfkit; anyone who knows Wharfkit should be able to use Dwarfkit without relearning it. Deviations forced by C++ or game engines (no exceptions, no promises, no browser) are listed in [DIVERGENCES.md](DIVERGENCES.md).

```cpp
#include <dwarfkit/dwarfkit.hpp>
namespace dk = dwarfkit;

auto session = /* SessionKit login, or restore */;
auto result = session->transact({.action = dk::Action::from({
    .account = "eosio.token"_n,
    .name = "transfer"_n,
    .authorization = {session->permissionLevel()},
    .data = Transfer{.from = session->actor(), .to = "teamgreymass"_n,
                     .quantity = "0.0001 EOS"_asset, .memo = "wharf!"},
})});
```

Status: under construction, ported module by module against the pinned upstream sources in `PORT_MANIFEST.md`. See [BLUEPRINT.md](BLUEPRINT.md) for the full plan and [PROGRESS.md](PROGRESS.md) for what works today.

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Requires CMake 3.24+ and MSVC 2022 / Clang 15+ / GCC 12+. Options: `DK_WITH_CURL` (default ON), `DK_BUILD_TESTS`, `DK_BUILD_TOOLS`, `DK_BUILD_EXAMPLES`, `DK_LIVE_TESTS`.

To hack on the port itself, first populate the upstream reference clones:

```
bash scripts/fetch_reference.sh
```
