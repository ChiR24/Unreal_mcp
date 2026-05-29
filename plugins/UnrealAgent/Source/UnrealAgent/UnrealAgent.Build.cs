using UnrealBuildTool;

public class UnrealAgent : ModuleRules
{
    public UnrealAgent(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "Slate",
            "SlateCore",
            "Json",
            "InputCore",
            "UnrealEd",
            "LevelEditor",
            "ToolMenus"
        });
    }
}
