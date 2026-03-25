// =============================================================================
// McpAutomationBridgeAI.Build.cs
// =============================================================================
// Build configuration for the AI optional module.
// This module only loads when optional AI plugins (StateTree, SmartObjects,
// MassEntity, MassSpawner) are enabled.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

using UnrealBuildTool;
using System.IO;

public class McpAutomationBridgeAI : ModuleRules
{
    public McpAutomationBridgeAI(ReadOnlyTargetRules Target) : base(Target)
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
            "GameplayTags",
            "AIModule"  // Required for AI controllers, EQS, behavior trees
        });

        // Conditional AI module dependencies
        // Use PrivateDependencyModuleNames to avoid forcing hard public imports on dependents
        string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

        // StateTree support
        bool bHasStateTree = Directory.Exists(Path.Combine(EngineDir, "Source", "Runtime", "StateTreeModule")) ||
                             Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "StateTree", "Source", "StateTreeModule")) ||
                             Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "StateTree"));
        if (bHasStateTree)
        {
            PrivateDependencyModuleNames.Add("StateTreeModule");
            PublicDefinitions.Add("MCP_STATETREE_MODULE_AVAILABLE=1");
            PublicDefinitions.Add("MCP_HAS_STATE_TREE=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_STATETREE_MODULE_AVAILABLE=0");
            PublicDefinitions.Add("MCP_HAS_STATE_TREE=0");
        }

        // SmartObjects support
        bool bHasSmartObjects = Directory.Exists(Path.Combine(EngineDir, "Source", "Runtime", "SmartObjectsModule")) ||
                                 Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "SmartObjects", "Source", "SmartObjectsModule")) ||
                                 Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "SmartObjects"));
        if (bHasSmartObjects)
        {
            PrivateDependencyModuleNames.Add("SmartObjectsModule");
            PublicDefinitions.Add("MCP_SMARTOBJECTS_MODULE_AVAILABLE=1");
            PublicDefinitions.Add("MCP_HAS_SMART_OBJECTS=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_SMARTOBJECTS_MODULE_AVAILABLE=0");
            PublicDefinitions.Add("MCP_HAS_SMART_OBJECTS=0");
        }

        // MassEntity support
        bool bHasMassEntity = Directory.Exists(Path.Combine(EngineDir, "Source", "Runtime", "MassEntity")) ||
                              Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "MassEntity", "Source", "MassEntity")) ||
                              Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "MassGameplay", "Source", "MassEntity"));
        if (bHasMassEntity)
        {
            PrivateDependencyModuleNames.Add("MassEntity");
            PublicDefinitions.Add("MCP_MASSENTITY_MODULE_AVAILABLE=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_MASSENTITY_MODULE_AVAILABLE=0");
        }

        // MassSpawner support
        bool bHasMassSpawner = Directory.Exists(Path.Combine(EngineDir, "Source", "Runtime", "MassSpawner")) ||
                                Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "MassGameplay", "Source", "MassSpawner"));
        if (bHasMassSpawner)
        {
            PrivateDependencyModuleNames.Add("MassSpawner");
            PublicDefinitions.Add("MCP_MASSSPAWNER_MODULE_AVAILABLE=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_MASSSPAWNER_MODULE_AVAILABLE=0");
        }

        // Set MCP_HAS_MASS_AI if any Mass module is available
        if (bHasMassEntity || bHasMassSpawner)
        {
            PublicDefinitions.Add("MCP_HAS_MASS_AI=1");
        }
        else
        {
            PublicDefinitions.Add("MCP_HAS_MASS_AI=0");
        }

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "Kismet",
                "AssetRegistry",
                "EditorScriptingUtilities",
                "BlueprintGraph",
                "AIGraph",
                "BehaviorTreeEditor",
                "EnvironmentQueryEditor"
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "KismetCompiler",
                "GameplayDebugger"
            });

            // Editor-specific AI modules (use PrivateDependency to avoid hard public imports)
            bool bHasStateTreeEditor = Directory.Exists(Path.Combine(EngineDir, "Source", "Editor", "StateTreeEditorModule")) ||
                                       Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "StateTree", "Source", "StateTreeEditorModule"));
            if (bHasStateTreeEditor)
            {
                PrivateDependencyModuleNames.Add("StateTreeEditorModule");
            }

            bool bHasSmartObjectsEditor = Directory.Exists(Path.Combine(EngineDir, "Source", "Editor", "SmartObjectsEditorModule")) ||
                                          Directory.Exists(Path.Combine(EngineDir, "Plugins", "Runtime", "SmartObjects", "Source", "SmartObjectsEditorModule"));
            if (bHasSmartObjectsEditor)
            {
                PrivateDependencyModuleNames.Add("SmartObjectsEditorModule");
            }
        }

        // Depend on base module for shared utilities and handler registration
        PrivateDependencyModuleNames.Add("McpAutomationBridge");
    }
}