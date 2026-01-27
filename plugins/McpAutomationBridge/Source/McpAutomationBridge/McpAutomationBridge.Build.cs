using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;

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
        
        // Disable PCH to prevent virtual memory exhaustion on systems with limited RAM
        // This is the most reliable workaround for C3859/C1076 errors
        bool enablePch = GetBoolEnvironmentVariable("MCP_ENABLE_PCH", true);
        if (enablePch)
        {
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
            PrivatePCHHeaderFile = "Private/McpAutomationBridgePCH.h";
        }
        else
        {
            PCHUsage = PCHUsageMode.NoPCHs;
        }
        
        // Unity builds enabled for faster compilation.
        bUseUnity = true;
        MinSourceFilesForUnityBuildOverride = 20;
        
        // Disable adaptive unity to ensure strict non-unity compilation
        TrySetBoolMember(this, "bUseAdaptiveUnityBuild", false);
        TrySetBoolMember(this, "bForceUnityBuild", false);
        TrySetBoolMember(this, "bMergeUnityFiles", false);
        TrySetBoolMember(this, "bFasterWithoutUnity", true);


        bool enableAdaptiveUnityTarget = GetBoolEnvironmentVariable("MCP_ENABLE_ADAPTIVE_UNITY", true);
        if (!enableAdaptiveUnityTarget)
        {
            TrySetBoolMember(Target, "bUseAdaptiveUnityBuild", false);
            TrySetBoolMember(Target, "bUseUnityBuild", true);
            TrySetBoolMember(Target, "bAdaptiveUnityDisablesPCH", false);
            TrySetBoolMember(Target, "bAdaptiveUnityDisablesOptimizations", false);
            TrySetBoolMember(Target, "bAdaptiveUnityCreatesDedicatedPCH", false);
        }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core","CoreUObject","Engine","Json","JsonUtilities",
            "LevelSequence", "MovieScene", "MovieSceneTracks", "GameplayTags",
            "CinematicCamera",  // For ACineCameraActor and UCineCameraComponent
            "DesktopPlatform",   // Phase 32: Build & Deployment (GenerateProjectFiles, RunUBT)
            "NetCore"            // For FFastArraySerializer networking support
        });

        if (Target.bBuildEditor)
        {
            // Editor-only Public Dependencies
            PublicDependencyModuleNames.AddRange(new string[] 
            { 
                "LevelSequenceEditor", "Sequencer", "MovieSceneTools", "Niagara", "NiagaraEditor", "UnrealEd",
                "WorldPartitionEditor", "DataLayerEditor", "EnhancedInput", "InputEditor",
                "EditorFramework"  // For FBuiltinEditorModes (EM_Default, EM_Landscape, etc.)
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ApplicationCore","Slate","SlateCore","Projects","InputCore","DeveloperSettings","Settings","EngineSettings",
                "Sockets","Networking","WebSockets","EditorSubsystem","EditorScriptingUtilities","BlueprintGraph",
                "Kismet","KismetCompiler","AssetRegistry","AssetTools","MaterialEditor","SourceControl",
                "AudioEditor", "DataValidation", "NiagaraEditor",
                // Phase 24: GAS, Audio, and missing module dependencies
                "GameplayAbilities",  // Required for UAttributeSet, UGameplayEffect, UGameplayAbility, etc.
                "AudioMixer",         // Required for FAudioEQEffect::ClampValues
                "CollectionManager"   // Required for ICollectionManager
                // Note: Interchange modules moved to conditional dependencies (TryAddConditionalModule)
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Landscape","LandscapeEditor","LandscapeEditorUtilities","Foliage","FoliageEdit",
                "AnimGraph","AnimationBlueprintLibrary","Persona","ToolMenus","EditorWidgets","PropertyEditor","LevelEditor",
                "ControlRig","ControlRigDeveloper","ControlRigEditor","RigVM","RigVMDeveloper","UMG","UMGEditor","ProceduralMeshComponent","MergeActors",
                "BehaviorTreeEditor", "RenderCore", "RHI", "AutomationController", "GameplayDebugger", "TraceLog", "TraceAnalysis", "AIModule", "AIGraph",
                "MeshUtilities", "MaterialUtilities", "PhysicsCore", "ClothingSystemRuntimeCommon",
                // Phase 6: Geometry Script (GeometryScripting plugin dependency in .uplugin ensures availability)
                "GeometryCore", "GeometryScriptingCore", "GeometryScriptingEditor", "GeometryFramework", "DynamicMesh", "MeshDescription", "StaticMeshDescription",
                // Phase 24: Navigation volumes
                "NavigationSystem",
                // Phase 44: Chaos Physics & Destruction
                "FieldSystemEngine", "GeometryCollectionEngine",
                // Phase 46: Modding & UGC
                "PakFile",
                // Phase 37: Interchange Framework (Runtime/Interchange/Engine)
                "InterchangeCore", "InterchangeEngine"
            });

            // --- Feature Detection Logic ---

            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // Phase 57: Version-specific defines
            if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 7)
            {
                PublicDefinitions.Add("MCP_UE_5_7_OR_LATER=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_UE_5_7_OR_LATER=0");
            }

            // Phase 11: MetaSound modules (conditional - may not be available in all UE versions)
            if (TryAddConditionalModule(Target, EngineDir, "MetasoundEngine", "MetasoundEngine"))
            {
                PublicDefinitions.Add("MCP_HAS_METASOUND=1");
                TryAddConditionalModule(Target, EngineDir, "MetasoundFrontend", "MetasoundFrontend");
                TryAddConditionalModule(Target, EngineDir, "MetasoundEditor", "MetasoundEditor");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_METASOUND=0");
            }

            // Phase 16: AI Systems - StateTree, SmartObjects, MassAI (conditional based on plugin availability)
            // These modules may not be available in all UE versions or plugin configurations
            if (TryAddConditionalModule(Target, EngineDir, "StateTreeModule", "StateTreeModule"))
            {
                PublicDefinitions.Add("MCP_HAS_STATETREE=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_STATETREE=0");
            }
            TryAddConditionalModule(Target, EngineDir, "StateTreeEditorModule", "StateTreeEditorModule");
            TryAddConditionalModule(Target, EngineDir, "SmartObjectsModule", "SmartObjectsModule");
            TryAddConditionalModule(Target, EngineDir, "SmartObjectsEditorModule", "SmartObjectsEditorModule");
            
            if (TryAddConditionalModule(Target, EngineDir, "MassEntity", "MassEntity"))
            {
                PublicDefinitions.Add("MCP_HAS_MASS=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_MASS=0");
            }
            TryAddConditionalModule(Target, EngineDir, "MassSpawner", "MassSpawner");
            TryAddConditionalModule(Target, EngineDir, "MassActors", "MassActors");

            // Phase 22: Voice Chat and Online Subsystem (conditional - for sessions handlers)
            // VoiceChat module is from the VoiceChat plugin
            TryAddConditionalModule(Target, EngineDir, "VoiceChat", "VoiceChat");
            // OnlineSubsystem provides IOnlineVoice for muting
            TryAddConditionalModule(Target, EngineDir, "OnlineSubsystem", "OnlineSubsystem");
            TryAddConditionalModule(Target, EngineDir, "OnlineSubsystemUtils", "OnlineSubsystemUtils");

            // Phase 27: PCG Framework (conditional - requires PCG plugin)
            if (TryAddConditionalModule(Target, EngineDir, "PCG", "PCG"))
            {
                PublicDefinitions.Add("MCP_HAS_PCG=1");
                TryAddConditionalModule(Target, EngineDir, "PCGEditor", "PCGEditor");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_PCG=0");
            }

            // Phase 3B: Motion Design (Avalanche)
            TryAddConditionalModule(Target, EngineDir, "MotionDesign", "MotionDesign");

            // Phase 3F: Animation & Motion
            // IK Rig
            if (TryAddConditionalModule(Target, EngineDir, "IKRig", "IKRig"))
            {
                PublicDefinitions.Add("MCP_HAS_IKRIG=1");
                TryAddConditionalModule(Target, EngineDir, "IKRigEditor", "IKRigEditor");
                TryAddConditionalModule(Target, EngineDir, "IKRigDeveloper", "IKRigDeveloper");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_IKRIG=0");
            }

            // Motion Warping
            if (TryAddConditionalModule(Target, EngineDir, "MotionWarping", "MotionWarping"))
            {
                PublicDefinitions.Add("MCP_HAS_MOTION_WARPING=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_MOTION_WARPING=0");
            }

            // Pose Search (Motion Matching)
            if (TryAddConditionalModule(Target, EngineDir, "PoseSearch", "PoseSearch"))
            {
                PublicDefinitions.Add("MCP_HAS_POSE_SEARCH=1");
                TryAddConditionalModule(Target, EngineDir, "PoseSearchEditor", "PoseSearchEditor");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_POSE_SEARCH=0");
            }

            // Animation Locomotion Library
            if (TryAddConditionalModule(Target, EngineDir, "AnimationLocomotionLibraryRuntime", "AnimationLocomotionLibrary"))
            {
                PublicDefinitions.Add("MCP_HAS_ANIM_LOCOMOTION=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_ANIM_LOCOMOTION=0");
            }

            // Phase 3F.2: Advanced Animation
            // MLDeformerFramework is an OPTIONAL plugin (may not be enabled in project).
            // Use DefineOptionalPluginMacro (macro-only, no DLL dependency) and __has_include in C++.
            DefineOptionalPluginMacro(EngineDir, "MLDeformerFramework", "MCP_HAS_MLDEFORMER");
            if (TryAddConditionalModule(Target, EngineDir, "AnimationModifiers", "AnimationModifiers"))
            {
                PublicDefinitions.Add("MCP_HAS_ANIM_MODIFIERS=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_ANIM_MODIFIERS=0");
            }
            DefineOptionalPluginMacro(EngineDir, "SkeletalMeshModelingTools", "MCP_HAS_SKEL_MESH_MODELING");

            // ============================================================================
            // OPTIONAL PLUGIN MODULES - DEFINE MACROS ONLY, NO LINK-TIME DEPENDENCIES
            // ============================================================================
            // These modules may exist in the engine but NOT be enabled in the current project.
            // Adding them as dependencies would cause DLL load failures at runtime.
            // Instead, we only define compile-time macros and use __has_include() in C++.
            // ============================================================================

            // Phase 28: Water Plugin (experimental)
            // Water is an optional experimental plugin - link it when available
            if (TryAddConditionalModule(Target, EngineDir, "Water", "Water"))
            {
                PublicDefinitions.Add("MCP_HAS_WATER_PLUGIN=1");
            }
            else
            {
                PublicDefinitions.Add("MCP_HAS_WATER_PLUGIN=0");
            }

            // Phase 30: Movie Render Pipeline (usually enabled, but still optional)
            DefineOptionalPluginMacro(EngineDir, "MovieRenderPipelineCore", "MCP_HAS_MOVIE_RENDER_PIPELINE");

            // Phase 3D: Chaos Physics & Vehicles
            DefineOptionalPluginMacro(EngineDir, "ChaosVehicles", "MCP_HAS_CHAOS_VEHICLES");
            DefineOptionalPluginMacro(EngineDir, "ChaosCloth", "MCP_HAS_CHAOS_CLOTH");
            DefineOptionalPluginMacro(EngineDir, "ChaosFlesh", "MCP_HAS_CHAOS_FLESH");

            // Phase 30: Media Framework (built-in, safe to add)
            TryAddConditionalModule(Target, EngineDir, "MediaAssets", "MediaAssets");

            // Phase 36: Character & Avatar Plugins
            DefineOptionalPluginMacro(EngineDir, "HairStrandsCore", "MCP_HAS_GROOM");
            DefineOptionalPluginMacro(EngineDir, "MutableRuntime", "MCP_HAS_MUTABLE");

            // Phase 37: Asset & Content Plugins
            // Interchange is built-in and always enabled in UE 5.0+, safe to keep
            // (Already added in PrivateDependencyModuleNames above)
            // USD - optional plugin
            DefineOptionalPluginMacro(EngineDir, "USDStage", "MCP_HAS_USD");
            // Alembic - optional plugin  
            DefineOptionalPluginMacro(EngineDir, "AlembicLibrary", "MCP_HAS_ALEMBIC");
            // glTF - optional Enterprise plugin
            DefineOptionalPluginMacro(EngineDir, "GLTFExporter", "MCP_HAS_GLTF");
            // Datasmith - optional Enterprise plugin
            DefineOptionalPluginMacro(EngineDir, "DatasmithCore", "MCP_HAS_DATASMITH");
            // SpeedTree - optional
            DefineOptionalPluginMacro(EngineDir, "SpeedTree", "MCP_HAS_SPEEDTREE");
            // Houdini Engine - external SideFX plugin
            DefineOptionalPluginMacro(EngineDir, "HoudiniEngineRuntime", "MCP_HAS_HOUDINI");
            // Substance - external Adobe plugin
            DefineOptionalPluginMacro(EngineDir, "SubstanceCore", "MCP_HAS_SUBSTANCE");

            // Phase 38: Audio Middleware Plugins
            // Bink Video - optional media plugin
            DefineOptionalPluginMacro(EngineDir, "BinkMediaPlayer", "MCP_HAS_BINK");
            // Wwise - external Audiokinetic plugin
            DefineOptionalPluginMacro(EngineDir, "AkAudio", "MCP_HAS_WWISE");
            // FMOD - external Firelight plugin
            DefineOptionalPluginMacro(EngineDir, "FMODStudio", "MCP_HAS_FMOD");

            // Phase 39: Live Link & Motion Capture
            // LiveLink is built-in and commonly enabled, but still check
            DefineOptionalPluginMacro(EngineDir, "LiveLinkInterface", "MCP_HAS_LIVELINK");

            // Phase 40: Virtual Production Plugins
            // nDisplay - optional virtual production plugin
            DefineOptionalPluginMacro(EngineDir, "DisplayCluster", "MCP_HAS_NDISPLAY");
            // Composure - optional compositing plugin
            DefineOptionalPluginMacro(EngineDir, "Composure", "MCP_HAS_COMPOSURE");
            // OpenColorIO - optional color management
            DefineOptionalPluginMacro(EngineDir, "OpenColorIO", "MCP_HAS_OCIO");
            // Remote Control - optional
            DefineOptionalPluginMacro(EngineDir, "RemoteControl", "MCP_HAS_REMOTE_CONTROL");
            // DMX - optional lighting control
            DefineOptionalPluginMacro(EngineDir, "DMXRuntime", "MCP_HAS_DMX");
            // OSC - optional networking
            DefineOptionalPluginMacro(EngineDir, "OSC", "MCP_HAS_OSC");
            // MIDI - optional
            DefineOptionalPluginMacro(EngineDir, "MIDIDevice", "MCP_HAS_MIDI");
            // Timecode/Genlock - commonly available
            TryAddConditionalModule(Target, EngineDir, "TimeManagement", "TimeManagement");

            // Phase 41: XR Plugins (VR/AR/MR)
            // HeadMountedDisplay - core XR, usually available
            DefineOptionalPluginMacro(EngineDir, "HeadMountedDisplay", "MCP_HAS_HMD");
            // OpenXR - optional
            DefineOptionalPluginMacro(EngineDir, "OpenXRHMD", "MCP_HAS_OPENXR");
            // Meta Quest / OculusXR - external Meta plugin
            DefineOptionalPluginMacro(EngineDir, "OculusXRHMD", "MCP_HAS_OCULUSXR");
            // SteamVR - external Valve plugin
            DefineOptionalPluginMacro(EngineDir, "SteamVR", "MCP_HAS_STEAMVR");
            // Apple ARKit - platform-specific
            DefineOptionalPluginMacro(EngineDir, "AppleARKit", "MCP_HAS_ARKIT");
            // Varjo - external plugin
            DefineOptionalPluginMacro(EngineDir, "VarjoHMD", "MCP_HAS_VARJO");
            // Windows Mixed Reality / HoloLens
            DefineOptionalPluginMacro(EngineDir, "WindowsMixedRealityHMD", "MCP_HAS_WMR");
            // Common AR module
            DefineOptionalPluginMacro(EngineDir, "AugmentedReality", "MCP_HAS_AR");
            // XRBase - core XR functionality  
            DefineOptionalPluginMacro(EngineDir, "XRBase", "MCP_HAS_XRBASE");

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

            // Control Rig Factory Support
            PublicDefinitions.Add("MCP_HAS_CONTROLRIG_FACTORY=1");
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
        if (maxDepth < 0 || string.IsNullOrEmpty(targetName) || !Directory.Exists(rootDir)) return false;

        try
        {
            HashSet<string> directoryIndex = GetDirectoryNameIndex(rootDir, maxDepth);
            return directoryIndex.Contains(targetName);
        }
        catch { /* Ignore access denied errors */ }
        return false;
    }

    private static HashSet<string> GetDirectoryNameIndex(string rootDir, int maxDepth)
    {
        string cacheKey = string.Concat(rootDir, "|", maxDepth.ToString());
        if (DirectoryIndexCache.TryGetValue(cacheKey, out HashSet<string> cached)) return cached;

        HashSet<string> index = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (maxDepth < 0 || string.IsNullOrEmpty(rootDir) || !Directory.Exists(rootDir))
        {
            DirectoryIndexCache[cacheKey] = index;
            return index;
        }

        int maxDepthInclusive = maxDepth + 1;
        Queue<(string Path, int Depth)> queue = new Queue<(string Path, int Depth)>();
        queue.Enqueue((rootDir, 0));

        while (queue.Count > 0)
        {
            (string currentDir, int depth) = queue.Dequeue();
            if (depth >= maxDepthInclusive) continue;

            string[] subDirs;
            try
            {
                subDirs = Directory.GetDirectories(currentDir);
            }
            catch
            {
                continue;
            }

            foreach (string subDir in subDirs)
            {
                string dirName = Path.GetFileName(subDir);
                if (!string.IsNullOrEmpty(dirName))
                {
                    index.Add(dirName);
                }

                if (depth + 1 <= maxDepthInclusive)
                {
                    queue.Enqueue((subDir, depth + 1));
                }
            }
        }

        DirectoryIndexCache[cacheKey] = index;
        return index;
    }

    /// <summary>
    /// Conditionally adds a module dependency if it exists in the engine or plugins directories.
    /// Used for optional AI modules that may not be available in all UE versions (StateTree, SmartObjects, MassEntity).
    /// </summary>
    /// <param name="Target">Build target settings.</param>
    /// <param name="EngineDir">Absolute path to the engine root directory.</param>
    /// <param name="ModuleName">The module name to add to dependencies if found.</param>
    /// <param name="SearchName">The directory name to search for in engine/plugin paths.</param>
    private bool TryAddConditionalModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName)
    {
        try
        {
            // Check Runtime modules
            string RuntimePath = Path.Combine(EngineDir, "Source", "Runtime", SearchName);
            if (Directory.Exists(RuntimePath))
            {
                PrivateDependencyModuleNames.Add(ModuleName);
                return true;
            }

            // Check Editor modules
            string EditorPath = Path.Combine(EngineDir, "Source", "Editor", SearchName);
            if (Directory.Exists(EditorPath))
            {
                PrivateDependencyModuleNames.Add(ModuleName);
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
                    Path.Combine(PluginsDir, "Runtime", "MassEntity", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "MassGameplay", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "SmartObjects", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "StateTree", "Source", SearchName),
                    // Phase 37: Asset & Content Plugin paths
                    Path.Combine(PluginsDir, "Importers", SearchName),
                    Path.Combine(PluginsDir, "Importers", "USDImporter", "Source", SearchName),
                    Path.Combine(PluginsDir, "Importers", "AlembicImporter", "Source", SearchName),
                    Path.Combine(PluginsDir, "Enterprise", SearchName),
                    Path.Combine(PluginsDir, "Enterprise", "GLTFExporter", "Source", SearchName),
                    Path.Combine(PluginsDir, "Enterprise", "DatasmithImporter", "Source", SearchName),
                    Path.Combine(PluginsDir, "Interchange", "Runtime", SearchName),
                    Path.Combine(PluginsDir, "Interchange", "Editor", SearchName),
                    Path.Combine(PluginsDir, "Editor", SearchName),
                    Path.Combine(PluginsDir, "FX", SearchName),
                    Path.Combine(PluginsDir, "Animation", SearchName),
                    // Animation plugins have nested module structures
                    Path.Combine(PluginsDir, "Animation", "IKRig", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "ControlRig", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "RigLogic", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "MLDeformer", "MLDeformerFramework", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "AnimationModifiers", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "AnimationWarping", "Source", SearchName),
                    Path.Combine(PluginsDir, "Animation", "PoseSearch", "Source", SearchName),
                    // Phase 40: Virtual Production Plugin paths
                    Path.Combine(PluginsDir, "VirtualProduction", SearchName),
                    Path.Combine(PluginsDir, "VirtualProduction", "DMX", "Source", SearchName),
                    Path.Combine(PluginsDir, "VirtualProduction", "RemoteControl", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "nDisplay", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "Composure", "Source", SearchName),
                    Path.Combine(PluginsDir, "Compositing", SearchName),
                    Path.Combine(PluginsDir, "Media", SearchName),
                    Path.Combine(PluginsDir, "Media", "AjaMedia", "Source", SearchName),
                    Path.Combine(PluginsDir, "Media", "BlackmagicMedia", "Source", SearchName),
                    Path.Combine(PluginsDir, "VirtualProduction", "Composure", "Source", SearchName),
                    Path.Combine(PluginsDir, "VirtualProduction", "OpenColorIO", "Source", SearchName),
                    // Phase 41: XR Plugin paths (VR/AR/MR)
                    Path.Combine(PluginsDir, "Runtime", "XR", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "OpenXR", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "OpenXRHMD", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "OculusXR", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "OculusVR", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "SteamVR", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "AppleARKit", "Source", SearchName),
                    // GoogleARCore removed - not available in UE 5.7
                    Path.Combine(PluginsDir, "Runtime", "Varjo", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "WindowsMixedReality", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "AR", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "AugmentedReality", "Source", SearchName),
                    Path.Combine(PluginsDir, "Runtime", "HeadMountedDisplay", "Source", SearchName)
                };

                foreach (string SearchPath in SearchPaths)
                {
                    if (Directory.Exists(SearchPath))
                    {
                        PrivateDependencyModuleNames.Add(ModuleName);
                        return true;
                    }
                }

                // Fallback: bounded depth search (max 4 levels) to avoid slow unbounded recursion
                if (SearchDirectoryBounded(PluginsDir, SearchName, 4))
                {
                    PrivateDependencyModuleNames.Add(ModuleName);
                    return true;
                }
            }
        }
        catch { /* Module not available - this is expected for optional modules */ }
        return false;
    }

    private static readonly Dictionary<string, HashSet<string>> DirectoryIndexCache =
        new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);

    private static bool GetBoolEnvironmentVariable(string name, bool defaultValue)
    {
        try
        {
            string value = Environment.GetEnvironmentVariable(name);
            if (string.IsNullOrWhiteSpace(value)) return defaultValue;

            return value.Equals("1", StringComparison.OrdinalIgnoreCase)
                || value.Equals("true", StringComparison.OrdinalIgnoreCase)
                || value.Equals("yes", StringComparison.OrdinalIgnoreCase)
                || value.Equals("on", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return defaultValue;
        }
    }

    private static void TrySetBoolMember(object target, string memberName, bool value)
    {
        if (target == null || string.IsNullOrWhiteSpace(memberName)) return;

        try
        {
            Type targetType = target.GetType();
            PropertyInfo prop = targetType.GetProperty(memberName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            if (prop != null && prop.PropertyType == typeof(bool))
            {
                MethodInfo setter = prop.GetSetMethod(true);
                if (setter != null)
                {
                    setter.Invoke(target, new object[] { value });
                    return;
                }
            }

            FieldInfo field = targetType.GetField(memberName, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            if (field != null && field.FieldType == typeof(bool))
            {
                field.SetValue(target, value);
            }
        }
        catch { /* Ignore reflection failures */ }
    }

    /// <summary>
    /// Defines a compile-time macro for an optional plugin WITHOUT adding it as a link-time dependency.
    /// This is critical for plugins that may exist in the engine but not be enabled in the project.
    /// The C++ code should use __has_include() or the defined macro to conditionally compile.
    /// </summary>
    /// <param name="EngineDir">Absolute path to the engine root directory.</param>
    /// <param name="ModuleName">The module name to search for.</param>
    /// <param name="MacroName">The macro to define if the module exists (e.g., "MCP_HAS_WATER").</param>
    private void DefineOptionalPluginMacro(string EngineDir, string ModuleName, string MacroName)
    {
        try
        {
            bool bFound = false;

            // Check Runtime modules
            string RuntimePath = Path.Combine(EngineDir, "Source", "Runtime", ModuleName);
            if (Directory.Exists(RuntimePath)) bFound = true;

            // Check Editor modules
            if (!bFound)
            {
                string EditorPath = Path.Combine(EngineDir, "Source", "Editor", ModuleName);
                if (Directory.Exists(EditorPath)) bFound = true;
            }

            // Check Plugins directory with bounded depth
            if (!bFound)
            {
                string PluginsDir = Path.Combine(EngineDir, "Plugins");
                if (Directory.Exists(PluginsDir) && SearchDirectoryBounded(PluginsDir, ModuleName, 4))
                {
                    bFound = true;
                }
            }

            // Define macro indicating availability (for __has_include fallback in C++)
            if (!string.IsNullOrEmpty(MacroName))
            {
                PublicDefinitions.Add(MacroName + "=" + (bFound ? "1" : "0"));
            }
        }
        catch { /* Ignore errors - just don't define the macro */ }
    }
}
