using UnrealBuildTool;

// The provider interface is header-only, so the sample takes EngineWire's include path and links nothing of it.
public class EngineWireSample : ModuleRules
{
    public EngineWireSample(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Json" });
        PrivateIncludePathModuleNames.Add("EngineWire");
    }
}
