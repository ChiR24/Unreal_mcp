using UnrealBuildTool;

public class McpDebugRuntime : ModuleRules
{
    public McpDebugRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Sockets",
            "Networking"
        });
        PublicDefinitions.Add(Target.Configuration == UnrealTargetConfiguration.Shipping
            ? "MCP_DEBUG_RUNTIME_ENABLED=0"
            : "MCP_DEBUG_RUNTIME_ENABLED=1");
    }
}
