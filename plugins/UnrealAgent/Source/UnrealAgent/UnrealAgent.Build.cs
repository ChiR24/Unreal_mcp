using UnrealBuildTool;
using System.IO;

public class UnrealAgent : ModuleRules
{
    public UnrealAgent(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = true;
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "Projects",
            "Slate",
            "SlateCore",
            "Json",
            "InputCore",
            "UnrealEd",
            "LevelEditor",
            "ToolMenus",
            "ContentBrowser",
            "AssetRegistry"
        });
    }
}
