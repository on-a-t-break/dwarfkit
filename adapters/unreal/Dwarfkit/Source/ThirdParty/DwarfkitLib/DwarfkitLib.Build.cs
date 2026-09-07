// Prebuilt dwarfkit static libraries + headers (BLUEPRINT.md 8.1).
//
// Build the library with curl OFF (the plugin supplies FHttp/FWebSockets
// transports) and the engine's CRT:
//   cmake -S <dwarfkit> -B build-ue -DDK_WITH_CURL=OFF -DDK_BUILD_TESTS=OFF \
//         -DDK_BUILD_TOOLS=OFF -DDK_BUILD_EXAMPLES=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
//   cmake --build build-ue --config Release --target dwarfkit
//
// dwarfkit links its dependencies privately, and a static archive never
// bundles what it depends on, so five archives are linked. Copy every
// archive the build produced into lib/<Platform>/ (find build-ue -name "*.lib"
// on Windows, "*.a" elsewhere):
//   dwarfkit, libsecp256k1 (named secp256k1 outside the MSVC generator),
//   secp256k1_precomputed, dk_trezor_crypto, dk_zlib
// The public headers include <tl/expected.hpp> and <nlohmann/json.hpp>, so
// copy the headers to this layout:
//   include/dwarfkit/**           <- the repo's include/dwarfkit
//   include/tl/expected.hpp       <- third_party/expected/include/tl
//   include/nlohmann/json.hpp     <- third_party/nlohmann/include/nlohmann
// (a verbatim copy of third_party/ under include/third_party/ also resolves).
// No compiler flags beyond C++20 are needed: the headers compile under both
// MSVC preprocessors and avoid the engine's macro names.
using System.IO;
using UnrealBuildTool;

public class DwarfkitLib : ModuleRules
{
    public DwarfkitLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string IncludeDir = Path.Combine(ModuleDirectory, "include");
        PublicSystemIncludePaths.Add(IncludeDir);
        PublicSystemIncludePaths.Add(Path.Combine(IncludeDir, "third_party", "expected", "include"));
        PublicSystemIncludePaths.Add(Path.Combine(IncludeDir, "third_party", "nlohmann", "include"));

        bool bWindows = Target.Platform == UnrealTargetPlatform.Win64;
        string LibDir = Path.Combine(ModuleDirectory, "lib", Target.Platform.ToString());
        string Prefix = bWindows ? "" : "lib";
        string Extension = bWindows ? ".lib" : ".a";
        string[] Archives =
        {
            "dwarfkit",
            bWindows ? "libsecp256k1" : "secp256k1",
            "secp256k1_precomputed",
            "dk_trezor_crypto",
            "dk_zlib",
        };
        foreach (string Archive in Archives)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, Prefix + Archive + Extension));
        }

        if (bWindows)
        {
            // the OS CSPRNG behind PrivateKey::generate
            PublicSystemLibraries.Add("bcrypt.lib");
        }
    }
}
