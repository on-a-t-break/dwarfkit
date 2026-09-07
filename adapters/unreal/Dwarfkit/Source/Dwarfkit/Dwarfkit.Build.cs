using UnrealBuildTool;

public class Dwarfkit : ModuleRules
{
    public Dwarfkit(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        // The dwarfkit library is built with exceptions enabled (its public
        // API is exception-free, but it guards throwing standard calls
        // internally), and header-only code shared across the boundary (the
        // STL, nlohmann::json, tl::expected) must be instantiated with the same
        // setting on both sides or it violates the ODR. Match the library.
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "DwarfkitLib"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP", "WebSockets"
        });
    }
}
