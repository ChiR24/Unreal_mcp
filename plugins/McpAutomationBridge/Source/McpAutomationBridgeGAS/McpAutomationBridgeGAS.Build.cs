// =============================================================================
// McpAutomationBridgeGAS.Build.cs
// =============================================================================
// Build configuration for the GAS (Gameplay Ability System) optional module.
// This module only loads when the GameplayAbilities plugin is enabled.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

using UnrealBuildTool;
using System.IO;

public class McpAutomationBridgeGAS : ModuleRules
{
    public McpAutomationBridgeGAS(ReadOnlyTargetRules Target) : base(Target)
    {
        // Match base module settings for reliable builds
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = true;

        // Core dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities",
            "GameplayTags"
        });

        // GAS dependency - this is the key module
        // Only add if GameplayAbilities is available
        string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
        bool bHasGAS = Directory.Exists(Path.Combine(EngineDir, "Source", "Runtime", "GameplayAbilities")) ||
                       Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "GameplayAbilities"));

        if (bHasGAS)
        {
            // Use PrivateDependency to avoid forcing hard public imports on dependents
            PrivateDependencyModuleNames.Add("GameplayAbilities");
            PublicDefinitions.Add("MCP_GAS_MODULE_AVAILABLE=1");
            // Set MCP_HAS_GAS=1 so base module handlers compile with GAS support
            PublicDefinitions.Add("MCP_HAS_GAS=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_GAS_MODULE_AVAILABLE=0");
            PublicDefinitions.Add("MCP_HAS_GAS=0");
        }

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "Kismet",
                "AssetRegistry",
                "EditorScriptingUtilities",
                "BlueprintGraph"
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "KismetCompiler"
            });
        }

        // Depend on base module for shared utilities and handler registration
        PrivateDependencyModuleNames.Add("McpAutomationBridge");
    }
}
