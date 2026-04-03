using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using EpicGames.Core;

public class McpAutomationBridge : ModuleRules
{
    // ============================================================================
    // NATIVE WINDOWS API FOR ACTUAL MEMORY DETECTION
    // ============================================================================
    [StructLayout(LayoutKind.Sequential)]
    private struct MEMORYSTATUSEX
    {
        internal uint dwLength;
        internal uint dwMemoryLoad;
        internal ulong ullTotalPhys;
        internal ulong ullAvailPhys;
        internal ulong ullTotalPageFile;
        internal ulong ullAvailPageFile;
        internal ulong ullTotalVirtual;
        internal ulong ullAvailVirtual;
        internal ulong ullAvailExtendedVirtual;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

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
        // Automatically adjust build parallelism based on ACTUAL available system memory
        // to prevent "compiler is out of heap space" errors (C1060)
        
        long AvailableMemoryMB = GetActualAvailableMemoryMB();
        long TotalMemoryMB = GetTotalPhysicalMemoryMB();
        
        // ============================================================================
        // UE 5.0-5.2 + MSVC: Disable undefined-identifier-to-errors and define __has_feature macro
        // ============================================================================
        // UE 5.0's TargetRules.bUndefinedIdentifierErrors defaults to true, which causes
        // UBT to add /we4668 to ALL C++ compilation commands (VCToolChain.cs:675).
        // The UE 5.0 engine header ConcurrentLinearAllocator.h line 26 uses the Clang-only
        // __has_feature macro. MSVC doesn't recognize it and emits C4668 (undefined macro
        // in #if directive). With /we4668, this becomes a fatal error.
        // We fix it at the root:
        //   1. Set bUndefinedIdentifierErrors = false to remove /we4668
        //   2. Add __has_feature(...)=0 to GlobalDefinitions so that the macro is defined
        //      for ALL compilations (including SharedPCH), and discards its arguments.
        // This propagates to GlobalCompileEnvironment and the SharedPCH compile command.
        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion <= 2)
        {
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                try
                {
                    var innerField = typeof(ReadOnlyTargetRules).GetField("Inner",
                        System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                    if (innerField != null)
                    {
                        var targetRules = (TargetRules)innerField.GetValue(Target);
                        if (targetRules != null)
                        {
#pragma warning disable 618  // bUndefinedIdentifierErrors is obsolete in UE5.5+, but we need it for older versions
                            if (targetRules.bUndefinedIdentifierErrors)
                            {
                                targetRules.bUndefinedIdentifierErrors = false;
                                Console.WriteLine("McpAutomationBridge: Disabled bUndefinedIdentifierErrors for UE 5.0-5.2 MSVC build");
                            }
#pragma warning restore 618

                            // Add __has_feature macro to global definitions (affects all compilations, including SharedPCH)
                            // Define as non-variadic macro that discards its argument, e.g., __has_feature(address_sanitizer) -> 0
                            const string HasFeatureDefine = "__has_feature(x)=0";
                            if (!targetRules.GlobalDefinitions.Contains(HasFeatureDefine))
                            {
                                targetRules.GlobalDefinitions.Add(HasFeatureDefine);
                                Console.WriteLine("McpAutomationBridge: Added __has_feature(x)=0 to GlobalDefinitions");
                            }
                        }
                    }
                }
                catch
                {
                    // Non-fatal; may succeed via alternative mechanisms
                }
                Console.WriteLine(string.Format("McpAutomationBridge: Applied MSVC __has_feature compatibility for UE 5.{0}", Target.Version.MinorVersion));
            }
        }

        // UBT already handles parallelism based on 1.5GB/action globally
        // Our job is to prevent HUGE compilation units that exceed heap space
        
        // IMPORTANT: Unity builds combine many .cpp files into one compilation unit
        // This causes each compiler process to need 3-6GB+ heap space instead of 1.5GB
        // For a module with 50+ handler files, unity builds cause heap exhaustion
        // even with plenty of RAM, because Windows page file space is limited
        
        Console.WriteLine(string.Format("McpAutomationBridge: Detected {0}MB available memory (of {1}MB total)", AvailableMemoryMB, TotalMemoryMB));
        
        // Disable PCH to prevent virtual memory exhaustion
        PCHUsage = PCHUsageMode.NoPCHs;
        
        // Unity builds enabled - combine files for faster compilation
        // Note: If you get "compiler out of heap space" errors, install BuildConfiguration.xml
        // from plugins/McpAutomationBridge/Config/BuildConfiguration.xml to %AppData%\Unreal Engine\UnrealBuildTool\
        bUseUnity = true;
        Console.WriteLine("McpAutomationBridge: Unity builds enabled");

PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core","CoreUObject","Engine","Json","JsonUtilities",
            "LevelSequence", "MovieScene", "MovieSceneTracks", "GameplayTags",
            "AIModule",  // Required for UEnvQueryTest_Distance and other EQS classes
            "Landscape"  // Required for FGrassVariety and other landscape classes
        });

        if (Target.bBuildEditor)
        {
            // Editor-only Public Dependencies (required for all editor builds)
            PublicDependencyModuleNames.AddRange(new string[] 
            { 
                "Sequencer", "MovieSceneTools", "Niagara", "UnrealEd",
                "WorldPartitionEditor", "DataLayerEditor",
                "MaterialEditor"  // UMaterialExpressionRotator and other material expressions
                // Optional plugins are handled by AddOptionalDynamicModule() below with delay-load
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ApplicationCore","Slate","SlateCore","Projects","InputCore","DeveloperSettings","Settings","EngineSettings",
                "Sockets","Networking","EditorSubsystem","EditorScriptingUtilities","BlueprintGraph","SSL",
                "Kismet","KismetCompiler","AssetRegistry","AssetTools","SourceControl",
                "AudioEditor", "AudioMixer"
                // Optional plugins are handled by AddOptionalDynamicModule() below with delay-load
            });

            // Add OpenSSL for TLS support (requires WITH_SSL)
            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LandscapeEditor","LandscapeEditorUtilities","Foliage","FoliageEdit",
                "AnimGraph","AnimationBlueprintLibrary","Persona","ToolMenus","EditorWidgets","PropertyEditor","LevelEditor",
                "RigVM","RigVMDeveloper","UMG","UMGEditor","MergeActors",
                "RenderCore", "RHI", "AutomationController", "GameplayDebugger", "TraceLog", "TraceAnalysis", "AIGraph",
                "MeshUtilities", "MaterialUtilities", "PhysicsCore", "ClothingSystemRuntimeCommon",
                "GeometryCore", "GeometryFramework", "DynamicMesh", "MeshDescription", "StaticMeshDescription",
                "NavigationSystem"
                // Optional plugins are handled by AddOptionalDynamicModule() below with delay-load
            });

            // --- Feature Detection Logic ---

            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // =========================================================================
            // OPTIONAL PLUGINS - Dynamic Loading
            // =========================================================================
            // All plugins marked Optional: true in .uplugin must use dynamic loading
            // to prevent hard DLL imports that fail when plugins are not enabled.

            // Phase 24: GAS - Gameplay Ability System (optional plugin)
            AddOptionalDynamicModule(Target, EngineDir, "GameplayAbilities", "GameplayAbilities");

            // Phase 11: MetaSound modules (optional plugin)
            // Note: MetasoundFrontend exports data symbols (FrontendInvalidID) so cannot use delay-load
            AddOptionalDynamicModule(Target, EngineDir, "MetasoundEngine", "MetasoundEngine");
            AddOptionalConditionalModule(Target, EngineDir, "MetasoundFrontend", "MetasoundFrontend");  // Has data symbols
            AddOptionalDynamicModule(Target, EngineDir, "MetasoundEditor", "MetasoundEditor");

            // Phase 16: AI Systems - StateTree, SmartObjects, MassAI (optional plugins)
            AddOptionalDynamicModule(Target, EngineDir, "StateTreeModule", "StateTreeModule");
            AddOptionalDynamicModule(Target, EngineDir, "StateTreeEditorModule", "StateTreeEditorModule");
            AddOptionalDynamicModule(Target, EngineDir, "SmartObjectsModule", "SmartObjectsModule");
            AddOptionalDynamicModule(Target, EngineDir, "SmartObjectsEditorModule", "SmartObjectsEditorModule");
            AddOptionalDynamicModule(Target, EngineDir, "MassEntity", "MassEntity");
            AddOptionalDynamicModule(Target, EngineDir, "MassSpawner", "MassSpawner");
            AddOptionalDynamicModule(Target, EngineDir, "MassActors", "MassActors");
            // Note: MassGameplay is a plugin name, not a module name. The MassGameplay plugin contains
            // modules like MassActors, MassSpawner, MassCommon, MassMovement, etc.

            // Phase 22: Online Subsystem (optional plugins)
            AddOptionalDynamicModule(Target, EngineDir, "OnlineSubsystem", "OnlineSubsystem");
            AddOptionalDynamicModule(Target, EngineDir, "OnlineSubsystemUtils", "OnlineSubsystemUtils");

            // ControlRig (optional plugin) - for animation rigging and IK
            AddOptionalDynamicModule(Target, EngineDir, "ControlRig", "ControlRig");
            AddOptionalDynamicModule(Target, EngineDir, "ControlRigDeveloper", "ControlRigDeveloper");
            AddOptionalDynamicModule(Target, EngineDir, "ControlRigEditor", "ControlRigEditor");

            // ProceduralMeshComponent (optional plugin) - for procedural geometry
            AddOptionalDynamicModule(Target, EngineDir, "ProceduralMeshComponent", "ProceduralMeshComponent");

            // EnvironmentQueryEditor (optional plugin) - for EQS authoring
            AddOptionalDynamicModule(Target, EngineDir, "EnvironmentQueryEditor", "EnvironmentQueryEditor");

            // GeometryScripting (optional plugin) - for geometry scripting
            AddOptionalDynamicModule(Target, EngineDir, "GeometryScriptingCore", "GeometryScriptingCore");
            AddOptionalDynamicModule(Target, EngineDir, "GeometryScriptingEditor", "GeometryScriptingEditor");

            // LevelSequenceEditor (optional plugin) - for Sequencer/Cinematics
            AddOptionalDynamicModule(Target, EngineDir, "LevelSequenceEditor", "LevelSequenceEditor");

            // NiagaraEditor (optional plugin) - for Niagara authoring
            AddOptionalDynamicModule(Target, EngineDir, "NiagaraEditor", "NiagaraEditor");

            // EnhancedInput (optional plugin) - for Enhanced Input system
            AddOptionalDynamicModule(Target, EngineDir, "EnhancedInput", "EnhancedInput");
            AddOptionalDynamicModule(Target, EngineDir, "InputEditor", "InputEditor");

            // BehaviorTreeEditor (optional plugin) - for Behavior Tree graph editing
            AddOptionalDynamicModule(Target, EngineDir, "BehaviorTreeEditor", "BehaviorTreeEditor");

            // DataValidation (optional plugin) - for data validation
            AddOptionalDynamicModule(Target, EngineDir, "DataValidation", "DataValidation");

            // Phase: IKRig and Vehicles (optional plugins)
            AddOptionalDynamicModule(Target, EngineDir, "IKRig", "IKRig");
            AddOptionalDynamicModule(Target, EngineDir, "IKRigEditor", "IKRigEditor");
            AddOptionalDynamicModule(Target, EngineDir, "ChaosVehicles", "ChaosVehicles");
            AddOptionalDynamicModule(Target, EngineDir, "AnimationData", "AnimationData");

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

        // ============================================================================
        // COMPILER WARNING SETTINGS (UE 5.6+ Compatibility)
        // ============================================================================
        // UE 5.6+ treats variable shadowing (C4456, C4458, C4459) as errors by default.
        // Use ShadowVariableWarningLevel directly on ModuleRules - works in all UE versions.
        // UE 5.0-5.5: Direct property on ModuleRules
        // UE 5.6-5.7: Forwarding property to CppCompileWarningSettings.ShadowVariableWarningLevel
        // This allows compilation while we systematically fix shadowing issues.
        // TODO: Fix variable shadowing in handler files, then remove this override
        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 6)
        {
            ShadowVariableWarningLevel = WarningLevel.Warning;
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
    /// Gets the ACTUAL available physical memory in MB using Windows API.
    /// Falls back to conservative estimate if detection fails.
    /// </summary>
    /// <returns>Available memory in MB.</returns>
    private long GetActualAvailableMemoryMB()
    {
        try
        {
            // Try Windows API first (most accurate)
            if (Environment.OSVersion.Platform == PlatformID.Win32NT)
            {
                var memStatus = new MEMORYSTATUSEX();
                memStatus.dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX));
                
                if (GlobalMemoryStatusEx(ref memStatus))
                {
                    // Return available physical memory in MB
                    return (long)(memStatus.ullAvailPhys / (1024 * 1024));
                }
            }
        }
        catch
        {
            // Fall through to heuristics
        }
        
        // Fallback: Check for environment variable hint
        string MemoryHint = Environment.GetEnvironmentVariable("UE_BUILD_MEMORY_MB");
        if (!string.IsNullOrEmpty(MemoryHint))
        {
            long HintValue;
            if (long.TryParse(MemoryHint, out HintValue) && HintValue > 0)
            {
                return HintValue;
            }
        }
        
        // Conservative fallback - assume 4GB available
        return 4096;
    }

    /// <summary>
    /// Gets the total physical memory in MB using Windows API.
    /// </summary>
    /// <returns>Total memory in MB.</returns>
    private long GetTotalPhysicalMemoryMB()
    {
        try
        {
            if (Environment.OSVersion.Platform == PlatformID.Win32NT)
            {
                var memStatus = new MEMORYSTATUSEX();
                memStatus.dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX));
                
                if (GlobalMemoryStatusEx(ref memStatus))
                {
                    return (long)(memStatus.ullTotalPhys / (1024 * 1024));
                }
            }
        }
        catch { }
        
        return 8192; // Conservative fallback
    }

    /// <summary>
    /// Searches for an optional module in standard engine and plugin locations.
    /// Checks Runtime/Editor source directories and common plugin subdirectories.
    /// </summary>
    /// <param name="EngineDir">Absolute path to the engine root directory.</param>
    /// <param name="SearchName">The directory name to search for.</param>
    /// <returns>True if the module directory was found, false otherwise.</returns>
    private bool FindOptionalModule(string EngineDir, string SearchName)
    {
        try
        {
            // Check Runtime modules
            string RuntimePath = Path.Combine(EngineDir, "Source", "Runtime", SearchName);
            if (Directory.Exists(RuntimePath))
            {
                return true;
            }

            // Check Editor modules
            string EditorPath = Path.Combine(EngineDir, "Source", "Editor", SearchName);
            if (Directory.Exists(EditorPath))
            {
                return true;
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
                    Path.Combine(PluginsDir, "Developer", SearchName),
                    Path.Combine(PluginsDir, "Animation", SearchName),
                    Path.Combine(PluginsDir, "Online", SearchName),  // OnlineSubsystem, VoiceChat
                    Path.Combine(PluginsDir, "Animation", "IKRig", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "ControlRig", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "MassEntity", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "MassGameplay", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "SmartObjects", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "StateTree", "Source", SearchName),
                    Path.Combine(PluginsDir, "Experimental", "ChaosVehiclesPlugin", "Source", SearchName)
                };

                foreach (string SearchPath in SearchPaths)
                {
                    if (Directory.Exists(SearchPath))
                    {
                        return true;
                    }
                }

                // Fallback: bounded depth search (max 4 levels)
                return SearchDirectoryBounded(PluginsDir, SearchName, 4);
            }
        }
        catch { /* Module not available - this is expected for optional modules */ }

        return false;
    }

    /// <summary>
    /// Adds an optional module conditionally - only if it exists.
    /// Used for optional AI modules that may not be available in all UE versions (StateTree, SmartObjects, MassEntity).
    /// </summary>
    /// <param name="Target">Build target settings.</param>
    /// <param name="EngineDir">Absolute path to the engine root directory.</param>
    /// <param name="ModuleName">The module name to add to dependencies if found.</param>
    /// <param name="SearchName">The directory name to search for in engine/plugin paths.</param>
    private void TryAddConditionalModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName)
    {
        if (FindOptionalModule(EngineDir, SearchName))
        {
            PrivateDependencyModuleNames.Add(ModuleName);
        }
    }

    /// <summary>
    /// Adds an optional module as a dependency with Windows delay-load support.
    /// 
    /// NOTE: Delay-load only works for function imports. Modules that export DATA symbols
    /// (variables, constants like `FrontendInvalidID`) cannot be delay-loaded and will fail
    /// with LNK1194. For those modules, use AddOptionalConditionalModule() instead.
    /// </summary>
    private bool AddOptionalDynamicModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName)
    {
        if (FindOptionalModule(EngineDir, SearchName))
        {
            // Add to PrivateDependencyModuleNames - this is REQUIRED for linking
            PrivateDependencyModuleNames.Add(ModuleName);

            // On Windows, add delay-load flag so the DLL loads even if optional plugins are missing
            // Note: This only works for function imports, not data symbols
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicDelayLoadDLLs.Add(string.Format("UnrealEditor-{0}.dll", ModuleName));
            }

            Console.WriteLine(string.Format("McpAutomationBridge: Added optional module '{0}' with delay-load", ModuleName));
            return true;
        }

        return false;
    }

    /// <summary>
    /// Adds an optional module conditionally - only if it exists.
    /// Use this for modules that export DATA symbols (cannot use delay-load).
    /// The plugin will only work in projects where these modules are enabled.
    /// </summary>
    private bool AddOptionalConditionalModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName)
    {
        if (FindOptionalModule(EngineDir, SearchName))
        {
            // Add as hard dependency - no delay-load
            PrivateDependencyModuleNames.Add(ModuleName);
            Console.WriteLine(string.Format("McpAutomationBridge: Added optional module '{0}' (conditional)", ModuleName));
            return true;
        }

        return false;
    }
}
