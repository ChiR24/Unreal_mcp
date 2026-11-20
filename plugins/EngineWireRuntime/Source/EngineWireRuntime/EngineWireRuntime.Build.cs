using UnrealBuildTool;

// A DeveloperTool module: it builds into editor, Development and Test targets and never into Shipping. It links no
// editor module and nothing of EngineWire, so a game build carries it alone.
public class EngineWireRuntime : ModuleRules
{
    public EngineWireRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Json", "Sockets", "Networking", "RenderCore", "RHI"
        });
    }
}
