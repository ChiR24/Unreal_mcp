using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class McpAutomationBridge : ModuleRules
{
    /// <summary>
    /// Configures build rules, dependencies, and compile-time feature definitions for the McpAutomationBridge module based on the provided build target.
    /// </summary>
    /// <param name="Target">Build target settings used to determine platform, configuration, and whether editor-only dependencies and feature flags should be enabled.</param>
    public McpAutomationBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        // ============================================================================
        // BUILD CONFIGURATION FOR 50+ HANDLER FILES
        // ============================================================================
        // Using NoPCHs to avoid "Failed to create virtual memory for PCH" errors
        // (C3859/C1076) that occur with large modules on systems with limited memory.
        // 
        // This trades slightly longer compile times for reliable builds without
        // requiring system paging file modifications.
        // ============================================================================
        
        // ============================================================================
        // DYNAMIC MEMORY-BASED BUILD CONFIGURATION
        // ============================================================================
        // Automatically adjust build parallelism based on available system memory
        // to prevent "compiler is out of heap space" errors (C1060)
        
        long AvailableMemoryMB = GetAvailableMemoryMB();
        bool bIsLowMemorySystem = AvailableMemoryMB < 8192; // Less than 8GB
        bool bIsVeryLowMemorySystem = AvailableMemoryMB < 4096; // Less than 4GB
        
        Console.WriteLine(string.Format("McpAutomationBridge: Detected {0}MB available memory", AvailableMemoryMB));
        
        // Disable PCH to prevent virtual memory exhaustion on systems with limited RAM
        // This is the most reliable workaround for C3859/C1076 errors
        PCHUsage = PCHUsageMode.NoPCHs;
        
        // Enable Unity builds for faster compilation on systems with sufficient memory
        // Unity builds combine multiple source files which speeds up compilation significantly
        // Only disable for very low memory systems (< 8GB) to prevent C1060 heap errors
        bUseUnity = !bIsLowMemorySystem;
        Console.WriteLine(string.Format("McpAutomationBridge: Unity builds {0}", bUseUnity ? "enabled" : "disabled (low memory system)"));
         
        // Disable Adaptive Unity to prevent files from being excluded from unity builds
        // bUseAdaptiveUnityBuild was removed in UE 5.7, use reflection to set it safely
        try
        {
            var prop = GetType().GetProperty("bUseAdaptiveUnityBuild");
            if (prop != null && !bIsVeryLowMemorySystem) { prop.SetValue(this, false); }
        }
        catch { /* Property doesn't exist in this UE version */ }
         
        // bMergeUnityFiles was also removed in UE 5.7
        try
        {
            var prop = GetType().GetProperty("bMergeUnityFiles");
            if (prop != null && !bIsLowMemorySystem) { prop.SetValue(this, true); }
        }
        catch { /* Property doesn't exist in this UE version */ }
        
        // Set max parallel actions based on available memory
        // Each compiler instance needs ~1-2GB of RAM
        try
        {
            var prop = GetType().GetProperty("MaxParallelActions");
            if (prop != null)
            {
                int MaxActions = bIsVeryLowMemorySystem ? 1 : (bIsLowMemorySystem ? 2 : 4);
                prop.SetValue(this, MaxActions);
                Console.WriteLine(string.Format("McpAutomationBridge: Max parallel actions set to {0}", MaxActions));
            }
        }
        catch { /* Property doesn't exist in this UE version */ }

        // UE 5.0 + MSVC: Suppress warnings from engine headers using Clang-only __has_feature macro
        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion == 0)
        {
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                // C4668: '__has_feature' is not defined as a preprocessor macro
                // C4067: unexpected tokens following preprocessor directive
                PublicDefinitions.Add("__has_feature(x)=0");
                Console.WriteLine("McpAutomationBridge: Added MSVC warning suppression for UE 5.0");
            }
        }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core","CoreUObject","Engine","Json","JsonUtilities",
            "LevelSequence", "MovieScene", "MovieSceneTracks", "GameplayTags"
        });

        if (Target.bBuildEditor)
        {
            // Editor-only Public Dependencies
            PublicDependencyModuleNames.AddRange(new string[] 
            { 
                "LevelSequenceEditor", "Sequencer", "MovieSceneTools", "Niagara", "NiagaraEditor", "UnrealEd",
                "WorldPartitionEditor", "DataLayerEditor", "EnhancedInput", "InputEditor"
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ApplicationCore","Slate","SlateCore","Projects","InputCore","DeveloperSettings","Settings","EngineSettings",
                "Sockets","Networking","EditorSubsystem","EditorScriptingUtilities","BlueprintGraph",
                "Kismet","KismetCompiler","AssetRegistry","AssetTools","MaterialEditor","SourceControl",
                "AudioEditor", "DataValidation", "NiagaraEditor",
                // Phase 24: GAS, Audio, and missing module dependencies
                "GameplayAbilities",  // Required for UAttributeSet, UGameplayEffect, UGameplayAbility, etc.
                "AudioMixer"          // Required for FAudioEQEffect::ClampValues
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Landscape","LandscapeEditor","LandscapeEditorUtilities","Foliage","FoliageEdit",
                "AnimGraph","AnimationBlueprintLibrary","Persona","ToolMenus","EditorWidgets","PropertyEditor","LevelEditor",
                "ControlRig","ControlRigDeveloper","ControlRigEditor","UMG","UMGEditor","ProceduralMeshComponent","MergeActors",
                "BehaviorTreeEditor", "EnvironmentQueryEditor", "RenderCore", "RHI", "AutomationController", "GameplayDebugger", "TraceLog", "TraceAnalysis", "AIModule", "AIGraph",
                "MeshUtilities", "MaterialUtilities", "PhysicsCore", "ClothingSystemRuntimeCommon",
                // Phase 6: Geometry Script (GeometryScripting plugin dependency in .uplugin ensures availability)
                "GeometryCore", "GeometryScriptingCore", "GeometryScriptingEditor", "GeometryFramework", "DynamicMesh", "MeshDescription", "StaticMeshDescription",
                // Phase 24: Navigation volumes
                "NavigationSystem"
            });

            // --- Feature Detection Logic ---

            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // Phase 11: MetaSound modules (conditional - may not be available in all UE versions)
            TryAddConditionalModule(Target, EngineDir, "MetasoundEngine", "MetasoundEngine");
            TryAddConditionalModule(Target, EngineDir, "MetasoundFrontend", "MetasoundFrontend");
            TryAddConditionalModule(Target, EngineDir, "MetasoundEditor", "MetasoundEditor");

            // Phase 16: AI Systems - StateTree, SmartObjects, MassAI (conditional based on plugin availability)
            // These modules may not be available in all UE versions or plugin configurations
            TryAddConditionalModule(Target, EngineDir, "StateTreeModule", "StateTreeModule");
            TryAddConditionalModule(Target, EngineDir, "StateTreeEditorModule", "StateTreeEditorModule");
            TryAddConditionalModule(Target, EngineDir, "SmartObjectsModule", "SmartObjectsModule");
            TryAddConditionalModule(Target, EngineDir, "SmartObjectsEditorModule", "SmartObjectsEditorModule");
            TryAddConditionalModule(Target, EngineDir, "MassEntity", "MassEntity");
            TryAddConditionalModule(Target, EngineDir, "MassSpawner", "MassSpawner");
            TryAddConditionalModule(Target, EngineDir, "MassActors", "MassActors");

            // Phase 22: Voice Chat and Online Subsystem (conditional - for sessions handlers)
            // VoiceChat module is from the VoiceChat plugin
            TryAddConditionalModule(Target, EngineDir, "VoiceChat", "VoiceChat");
            // OnlineSubsystem provides IOnlineVoice for muting
            TryAddConditionalModule(Target, EngineDir, "OnlineSubsystem", "OnlineSubsystem");
            TryAddConditionalModule(Target, EngineDir, "OnlineSubsystemUtils", "OnlineSubsystemUtils");

            // Ensure editor builds expose full Blueprint graph editing APIs.
            PublicDefinitions.Add("MCP_HAS_K2NODE_HEADERS=1");
            PublicDefinitions.Add("MCP_HAS_EDGRAPH_SCHEMA_K2=1");

            // 1. SubobjectData Detection
            // UE 5.7 renamed/moved this to SubobjectDataInterface in Editor/
            bool bHasSubobjectDataInterface = Directory.Exists(Path.Combine(EngineDir, "Source", "Editor", "SubobjectDataInterface"));
            
            if (bHasSubobjectDataInterface)
            {
                PrivateDependencyModuleNames.Add("SubobjectDataInterface");
                PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
            }
            else
            {
                // Fallback for older versions
                if (!PrivateDependencyModuleNames.Contains("SubobjectData"))
                {
                    PrivateDependencyModuleNames.Add("SubobjectData");
                }
                PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
            }

            // 2. WorldPartition Support Detection
            // Detect whether UWorldPartition supports ForEachDataLayerInstance
            bool bHasWPForEach = false;
            try
            {
                // In UE 5.7, ForEachDataLayerInstance moved to DataLayerManager.h
                string WPHeader = Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public", "WorldPartition", "DataLayer", "DataLayerManager.h");
                if (!File.Exists(WPHeader))
                {
                    // Fallback to old location for older engines
                    WPHeader = Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public", "WorldPartition", "WorldPartition.h");
                }

                if (File.Exists(WPHeader))
                {
                    string Content = File.ReadAllText(WPHeader);
                    if (Content.Contains("ForEachDataLayerInstance("))
                    {
                        bHasWPForEach = true;
                    }
                }
            }
            catch {}

            PublicDefinitions.Add(bHasWPForEach ? "MCP_HAS_WP_FOR_EACH_DATALAYER=1" : "MCP_HAS_WP_FOR_EACH_DATALAYER=0");

            // Ensure Win64 debug builds emit Edit-and-Continue friendly debug info
            if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration == UnrealTargetConfiguration.Debug)
            {
                PublicDefinitions.Add("MCP_ENABLE_EDIT_AND_CONTINUE=1");
            }

            // Control Rig Factory Support - detection is handled in source code via __has_include
            // Do not define MCP_HAS_CONTROLRIG_FACTORY here to avoid redefinition warnings
        }
        else
        {
            // Non-editor builds cannot rely on editor-only headers.
            PublicDefinitions.Add("MCP_HAS_K2NODE_HEADERS=0");
            PublicDefinitions.Add("MCP_HAS_EDGRAPH_SCHEMA_K2=0");
            PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=0");
            PublicDefinitions.Add("MCP_HAS_WP_FOR_EACH_DATALAYER=0");
        }
    }

    /// <summary>
    /// Determines whether a SubobjectData module or plugin exists under the given engine directory.
    /// </summary>
    /// <param name="EngineDir">Absolute path to the engine root directory to inspect.</param>
    /// <returns>`true` if a SubobjectData directory is found in EngineDir/Source/Runtime/SubobjectData or in known plugin locations; `false` if not found or if an error occurs.</returns>
    private bool IsSubobjectDataAvailable(string EngineDir)
    {
        try
        {
            if (string.IsNullOrEmpty(EngineDir)) return false;
            
            // Check Runtime module
            string RuntimeDir = Path.Combine(EngineDir, "Source", "Runtime", "SubobjectData");
            if (Directory.Exists(RuntimeDir)) return true;

            // Check Editor module (UE 5.7+)
            string EditorDir = Path.Combine(EngineDir, "Source", "Editor", "SubobjectDataInterface");
            if (Directory.Exists(EditorDir)) return true;

            // Check known plugin locations with bounded depth search
            string PluginsDir = Path.Combine(EngineDir, "Plugins");
            if (Directory.Exists(PluginsDir))
            {
                // Check common plugin locations first (fast path)
                string[] KnownPaths = new string[]
                {
                    Path.Combine(PluginsDir, "Runtime", "SubobjectData"),
                    Path.Combine(PluginsDir, "Editor", "SubobjectData"),
                    Path.Combine(PluginsDir, "Experimental", "SubobjectData")
                };
                foreach (string path in KnownPaths)
                {
                    if (Directory.Exists(path)) return true;
                }

                // Bounded depth search (max 3 levels deep) to avoid slow unbounded recursion
                if (SearchDirectoryBounded(PluginsDir, "SubobjectData", 3)) return true;
            }
        }
        catch {}
        return false;
    }

    /// <summary>
    /// Determines whether the current project contains a "SubobjectData" directory inside its Plugins folder by searching upward from the provided module directory for the project root (.uproject).
    /// </summary>
    /// <param name="ModuleDir">Path to the module directory used as the starting point to locate the project root.</param>
    /// <returns>`true` if a "SubobjectData" directory is found under the project's Plugins directory, `false` otherwise.</returns>
    private bool IsSubobjectDataInProject(string ModuleDir)
    {
        try
        {
            // Find project root by looking for .uproject
            string ProjectRoot = null;
            DirectoryInfo Dir = new DirectoryInfo(ModuleDir);
            while (Dir != null)
            {
                if (Dir.GetFiles("*.uproject").Length > 0) 
                { 
                    ProjectRoot = Dir.FullName; 
                    break; 
                }
                Dir = Dir.Parent;
            }

            if (!string.IsNullOrEmpty(ProjectRoot))
            {
                string ProjPlugins = Path.Combine(ProjectRoot, "Plugins");
                if (Directory.Exists(ProjPlugins))
                {
                    // Use bounded depth search (max 3 levels) to avoid slow unbounded recursion
                    if (SearchDirectoryBounded(ProjPlugins, "SubobjectData", 3)) return true;
                }
            }
        }
        catch {}
        return false;
    }

    /// <summary>
    /// Searches for a directory with the given name up to a maximum depth.
    /// </summary>
    /// <param name="rootDir">The root directory to start searching from.</param>
    /// <param name="targetName">The directory name to search for.</param>
    /// <param name="maxDepth">Maximum depth to search (0 = root only).</param>
    /// <returns>True if directory is found within the depth limit.</returns>
    private bool SearchDirectoryBounded(string rootDir, string targetName, int maxDepth)
    {
        if (maxDepth < 0 || !Directory.Exists(rootDir)) return false;
        
        try
        {
            foreach (string subDir in Directory.GetDirectories(rootDir))
            {
                string dirName = Path.GetFileName(subDir);
                if (string.Equals(dirName, targetName, StringComparison.OrdinalIgnoreCase))
                    return true;
                
                if (maxDepth > 0 && SearchDirectoryBounded(subDir, targetName, maxDepth - 1))
                    return true;
            }
        }
        catch { /* Ignore access denied errors */ }
        return false;
    }

    /// <summary>
    /// Gets the approximate available physical memory in MB.
    /// Uses a simple heuristic based on environment and process info.
    /// </summary>
    /// <returns>Available memory in MB.</returns>
    private long GetAvailableMemoryMB()
    {
        try
        {
            // Check for UE_BUILD_CONFIGURATION environment variable
            // This can be set to hint at memory constraints
            string MemoryHint = Environment.GetEnvironmentVariable("UE_BUILD_MEMORY_MB");
            if (!string.IsNullOrEmpty(MemoryHint))
            {
                long HintValue;
                if (long.TryParse(MemoryHint, out HintValue) && HintValue > 0)
                {
                    return HintValue;
                }
            }
            
            // Check for MSBuild's max CPU count - if low, system might be constrained
            string ProcessorCount = Environment.GetEnvironmentVariable("NUMBER_OF_PROCESSORS");
            if (!string.IsNullOrEmpty(ProcessorCount))
            {
                int CpuCount;
                if (int.TryParse(ProcessorCount, out CpuCount))
                {
                    // Rough heuristic: assume 2GB per core minimum, 4GB per core recommended
                    // For systems with many cores, assume more RAM
                    long EstimatedMemory = CpuCount * 2048; // 2GB per core minimum
                    
                    // Cap the estimate to reasonable bounds
                    if (EstimatedMemory > 65536) return 65536; // Max 64GB
                    if (EstimatedMemory < 4096) return 4096;   // Min 4GB
                    
                    return EstimatedMemory;
                }
            }
            
            // Conservative default
            return 8192; // Assume 8GB if we can't determine
        }
        catch
        {
            // Default to conservative estimate
            return 8192; // Assume 8GB
        }
    }

    /// <summary>
    /// Conditionally adds a module dependency if it exists in the engine or plugins directories.
    /// Used for optional AI modules that may not be available in all UE versions (StateTree, SmartObjects, MassEntity).
    /// </summary>
    /// <param name="Target">Build target settings.</param>
    /// <param name="EngineDir">Absolute path to the engine root directory.</param>
    /// <param name="ModuleName">The module name to add to dependencies if found.</param>
    /// <param name="SearchName">The directory name to search for in engine/plugin paths.</param>
    private void TryAddConditionalModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName)
    {
        try
        {
            // Check Runtime modules
            string RuntimePath = Path.Combine(EngineDir, "Source", "Runtime", SearchName);
            if (Directory.Exists(RuntimePath))
            {
                PrivateDependencyModuleNames.Add(ModuleName);
                return;
            }

            // Check Editor modules
            string EditorPath = Path.Combine(EngineDir, "Source", "Editor", SearchName);
            if (Directory.Exists(EditorPath))
            {
                PrivateDependencyModuleNames.Add(ModuleName);
                return;
            }

            // Check Plugins directory
            string PluginsDir = Path.Combine(EngineDir, "Plugins");
            if (Directory.Exists(PluginsDir))
            {
                // Check common plugin locations
                string[] SearchPaths = new string[]
                {
                    Path.Combine(PluginsDir, "AI", SearchName),
                    Path.Combine(PluginsDir, "Runtime", SearchName),
                    Path.Combine(PluginsDir, "Experimental", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "MassEntity", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "MassGameplay", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "SmartObjects", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "StateTree", "Source", SearchName)
                };

                foreach (string SearchPath in SearchPaths)
                {
                    if (Directory.Exists(SearchPath))
                    {
                        PrivateDependencyModuleNames.Add(ModuleName);
                        return;
                    }
                }

                // Fallback: bounded depth search (max 4 levels) to avoid slow unbounded recursion
                if (SearchDirectoryBounded(PluginsDir, SearchName, 4))
                {
                    PrivateDependencyModuleNames.Add(ModuleName);
                    return;
                }
            }
        }
        catch { /* Module not available - this is expected for optional modules */ }
    }
}
