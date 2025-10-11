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
            "Json",
            "JsonUtilities"
        });

        // Editor-only dependencies should only be added when building for the editor target
        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.Add("EditorSubsystem");
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ApplicationCore",
                "Slate",
                "SlateCore",
                "Projects",
                "InputCore",
                "DeveloperSettings",
                "Settings",
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

            // Sequencer / LevelSequence editor modules (editor-only)
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LevelSequence",
                "LevelSequenceEditor",
                "Sequencer",
                "MovieScene",
                "MovieSceneTools"
            });
        }
    }
}
