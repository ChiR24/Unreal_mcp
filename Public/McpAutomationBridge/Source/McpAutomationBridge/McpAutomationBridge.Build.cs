using UnrealBuildTool;

public class McpAutomationBridge : ModuleRules
{
    public McpAutomationBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "EditorSubsystem",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "Slate",
            "SlateCore",
            "Projects",
            "InputCore",
            "DeveloperSettings",
            "Sockets",
            "Networking",
            "UnrealEd",
            "PythonScriptPlugin",
            "EditorScriptingUtilities",
            "BlueprintGraph",
            "Kismet",
            "KismetCompiler",
            "AssetRegistry",
            "AssetTools"
        });
    }
}
