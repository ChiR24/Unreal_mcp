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
        
        // Disable PCH to prevent virtual memory exhaustion on systems with limited RAM
        // This is the most reliable workaround for C3859/C1076 errors
        PCHUsage = PCHUsageMode.NoPCHs;
        
        // Enable Unity builds to reduce compilation units
        bUseUnity = true;
        MinSourceFilesForUnityBuildOverride = 2;
        
        // Increase bytes per unity file to combine MORE files into fewer translation units
        // Default is 384KB; we increase to 2MB to handle our 50+ handler files
        // This reduces number of compiler invocations, preventing memory exhaustion
        NumIncludedBytesPerUnityCPPOverride = 2048 * 1024;
        
        // Disable Adaptive Unity to prevent files from being excluded from unity builds
        // bUseAdaptiveUnityBuild was removed in UE 5.7, use reflection to set it safely
        try
        {
            var prop = GetType().GetProperty("bUseAdaptiveUnityBuild");
            if (prop != null) { prop.SetValue(this, false); }
        }
        catch { /* Property doesn't exist in this UE version */ }
        
        // bMergeUnityFiles was also removed in UE 5.7
        try
        {
            var prop = GetType().GetProperty("bMergeUnityFiles");
            if (prop != null) { prop.SetValue(this, true); }
        }
        catch { /* Property doesn't exist in this UE version */ }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core","CoreUObject","Engine","Json","JsonUtilities",
            "LevelSequence", "MovieScene", "MovieSceneTracks", "GameplayTags",
            "CinematicCamera",  // For ACineCameraActor and UCineCameraComponent
            "DesktopPlatform"   // Phase 32: Build & Deployment (GenerateProjectFiles, RunUBT)
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
                "Sockets","Networking","EditorSubsystem","EditorScriptingUtilities","BlueprintGraph",
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

            // Phase 27: PCG Framework (conditional - requires PCG plugin)
            TryAddConditionalModule(Target, EngineDir, "PCG", "PCG");
            TryAddConditionalModule(Target, EngineDir, "PCGEditor", "PCGEditor");

            // Phase 28: Water Plugin (conditional - experimental plugin)
            TryAddConditionalModule(Target, EngineDir, "Water", "Water");
            TryAddConditionalModule(Target, EngineDir, "WaterEditor", "WaterEditor");

            // Phase 30: Movie Render Pipeline (conditional - for cinematics/rendering)
            TryAddConditionalModule(Target, EngineDir, "MovieRenderPipelineCore", "MovieRenderPipelineCore");
            TryAddConditionalModule(Target, EngineDir, "MovieRenderPipelineSettings", "MovieRenderPipelineSettings");
            TryAddConditionalModule(Target, EngineDir, "MovieRenderPipelineRenderPasses", "MovieRenderPipelineRenderPasses");

            // Phase 30: Media Framework
            TryAddConditionalModule(Target, EngineDir, "MediaAssets", "MediaAssets");

            // Phase 36: Character & Avatar Plugins
            // Groom/HairStrands (conditional - requires HairStrands plugin)
            TryAddConditionalModule(Target, EngineDir, "HairStrandsCore", "HairStrandsCore");
            TryAddConditionalModule(Target, EngineDir, "HairStrandsEditor", "HairStrandsEditor");
            // Note: Niagara is already added as a public dependency on line 67, no duplicate needed
            // Mutable/Customizable (conditional - requires Mutable plugin)
            TryAddConditionalModule(Target, EngineDir, "MutableRuntime", "MutableRuntime");
            TryAddConditionalModule(Target, EngineDir, "MutableTools", "MutableTools");

            // Phase 37: Asset & Content Plugins
            // Interchange Framework (built-in for UE 5.0+)
            TryAddConditionalModule(Target, EngineDir, "InterchangeCore", "InterchangeCore");
            TryAddConditionalModule(Target, EngineDir, "InterchangeEngine", "InterchangeEngine");
            TryAddConditionalModule(Target, EngineDir, "InterchangeNodes", "InterchangeNodes");
            TryAddConditionalModule(Target, EngineDir, "InterchangePipelines", "InterchangePipelines");
            // USD (built-in plugin) - Module names match directory names
            TryAddConditionalModule(Target, EngineDir, "USDClasses", "USDClasses");
            TryAddConditionalModule(Target, EngineDir, "USDStage", "USDStage");
            TryAddConditionalModule(Target, EngineDir, "USDStageEditor", "USDStageEditor");
            TryAddConditionalModule(Target, EngineDir, "USDSchemas", "USDSchemas");
            TryAddConditionalModule(Target, EngineDir, "USDExporter", "USDExporter");
            // Note: No USDImporter module - it's a plugin folder, not a module
            // Alembic (built-in plugin)
            TryAddConditionalModule(Target, EngineDir, "AlembicLibrary", "AlembicLibrary");
            // Note: No AlembicImporter module - functionality is in AlembicLibrary
            // glTF (built-in Enterprise plugin)
            TryAddConditionalModule(Target, EngineDir, "GLTFExporter", "GLTFExporter");
            TryAddConditionalModule(Target, EngineDir, "GLTFCore", "GLTFCore");
            // Datasmith (built-in Enterprise plugin)
            TryAddConditionalModule(Target, EngineDir, "DatasmithCore", "DatasmithCore");
            TryAddConditionalModule(Target, EngineDir, "DatasmithContent", "DatasmithContent");
            // Note: DatasmithTranslator, DatasmithImporter, DatasmithExporter may not exist as separate modules
            // SpeedTree (built-in)
            TryAddConditionalModule(Target, EngineDir, "SpeedTree", "SpeedTree");
            // Fab/Quixel (external - Epic Games Launcher installed)
            TryAddConditionalModule(Target, EngineDir, "QuixelBridge", "QuixelBridge");
            // Houdini Engine (external - SideFX installed)
            TryAddConditionalModule(Target, EngineDir, "HoudiniEngineRuntime", "HoudiniEngineRuntime");
            TryAddConditionalModule(Target, EngineDir, "HoudiniEngineEditor", "HoudiniEngineEditor");
            // Substance (external - Adobe installed)
            TryAddConditionalModule(Target, EngineDir, "SubstanceCore", "SubstanceCore");
            TryAddConditionalModule(Target, EngineDir, "SubstanceEditor", "SubstanceEditor");

            // Phase 38: Audio Middleware Plugins
            // Bink Video (built-in for UE 5.0+)
            TryAddConditionalModule(Target, EngineDir, "BinkMediaPlayer", "BinkMediaPlayer");
            // Wwise (external - Audiokinetic installed)
            TryAddConditionalModule(Target, EngineDir, "AkAudio", "AkAudio");
            TryAddConditionalModule(Target, EngineDir, "AkAudioEditor", "AkAudioEditor");
            // FMOD (external - Firelight Technologies installed)
            TryAddConditionalModule(Target, EngineDir, "FMODStudio", "FMODStudio");
            TryAddConditionalModule(Target, EngineDir, "FMODStudioEditor", "FMODStudioEditor");

            // Phase 39: Live Link & Motion Capture
            // LiveLinkInterface (built-in since UE 4.19+)
            TryAddConditionalModule(Target, EngineDir, "LiveLinkInterface", "LiveLinkInterface");
            // LiveLink (main plugin module)
            TryAddConditionalModule(Target, EngineDir, "LiveLink", "LiveLink");
            // LiveLinkComponents (component controllers)
            TryAddConditionalModule(Target, EngineDir, "LiveLinkComponents", "LiveLinkComponents");
            // LiveLinkEditor (editor integration)
            TryAddConditionalModule(Target, EngineDir, "LiveLinkEditor", "LiveLinkEditor");
            // LiveLinkMessageBusFramework (network discovery)
            TryAddConditionalModule(Target, EngineDir, "LiveLinkMessageBusFramework", "LiveLinkMessageBusFramework");

            // Phase 40: Virtual Production Plugins
            // nDisplay (built-in for UE 4.22+)
            TryAddConditionalModule(Target, EngineDir, "DisplayCluster", "DisplayCluster");
            TryAddConditionalModule(Target, EngineDir, "DisplayClusterConfiguration", "DisplayClusterConfiguration");
            // Composure (built-in compositing plugin)
            TryAddConditionalModule(Target, EngineDir, "Composure", "Composure");
            TryAddConditionalModule(Target, EngineDir, "CompositorSources", "CompositorSources");
            // OpenColorIO (built-in for UE 4.27+)
            TryAddConditionalModule(Target, EngineDir, "OpenColorIO", "OpenColorIO");
            // Remote Control (built-in for UE 4.25+)
            TryAddConditionalModule(Target, EngineDir, "RemoteControl", "RemoteControl");
            TryAddConditionalModule(Target, EngineDir, "RemoteControlCommon", "RemoteControlCommon");
            TryAddConditionalModule(Target, EngineDir, "WebRemoteControl", "WebRemoteControl");
            // DMX (built-in for UE 4.27+)
            TryAddConditionalModule(Target, EngineDir, "DMXProtocol", "DMXProtocol");
            TryAddConditionalModule(Target, EngineDir, "DMXProtocolArtNet", "DMXProtocolArtNet");
            TryAddConditionalModule(Target, EngineDir, "DMXProtocolSACN", "DMXProtocolSACN");
            TryAddConditionalModule(Target, EngineDir, "DMXRuntime", "DMXRuntime");
            // OSC (built-in)
            TryAddConditionalModule(Target, EngineDir, "OSC", "OSC");
            // MIDI (built-in for UE 4.21+)
            TryAddConditionalModule(Target, EngineDir, "MIDIDevice", "MIDIDevice");
            // Timecode/Genlock (built-in)
            TryAddConditionalModule(Target, EngineDir, "TimeManagement", "TimeManagement");
            // AJA Media (optional - professional I/O)
            TryAddConditionalModule(Target, EngineDir, "AjaMedia", "AjaMedia");
            TryAddConditionalModule(Target, EngineDir, "AjaMediaOutput", "AjaMediaOutput");
            // Blackmagic Media (optional - professional I/O)
            TryAddConditionalModule(Target, EngineDir, "BlackmagicMedia", "BlackmagicMedia");
            TryAddConditionalModule(Target, EngineDir, "BlackmagicMediaOutput", "BlackmagicMediaOutput");

            // Phase 41: XR Plugins (VR/AR/MR)
            // HeadMountedDisplay (built-in core XR)
            TryAddConditionalModule(Target, EngineDir, "HeadMountedDisplay", "HeadMountedDisplay");
            // OpenXR (built-in for UE 4.27+)
            TryAddConditionalModule(Target, EngineDir, "OpenXRHMD", "OpenXRHMD");
            TryAddConditionalModule(Target, EngineDir, "OpenXRInput", "OpenXRInput");
            // OculusXR / Meta Quest (optional - Meta plugin)
            TryAddConditionalModule(Target, EngineDir, "OculusXRHMD", "OculusXRHMD");
            TryAddConditionalModule(Target, EngineDir, "OculusXRInput", "OculusXRInput");
            TryAddConditionalModule(Target, EngineDir, "OculusXRPassthrough", "OculusXRPassthrough");
            TryAddConditionalModule(Target, EngineDir, "OculusXRAnchors", "OculusXRAnchors");
            // SteamVR (optional - Valve plugin)
            TryAddConditionalModule(Target, EngineDir, "SteamVR", "SteamVR");
            TryAddConditionalModule(Target, EngineDir, "SteamVRInputDevice", "SteamVRInputDevice");
            // Apple ARKit (built-in for iOS)
            TryAddConditionalModule(Target, EngineDir, "AppleARKit", "AppleARKit");
            TryAddConditionalModule(Target, EngineDir, "AppleARKitFaceSupport", "AppleARKitFaceSupport");
            // Google ARCore (built-in for Android)
            TryAddConditionalModule(Target, EngineDir, "GoogleARCore", "GoogleARCore");
            TryAddConditionalModule(Target, EngineDir, "GoogleARCoreBase", "GoogleARCoreBase");
            // Varjo (optional - Varjo plugin)
            TryAddConditionalModule(Target, EngineDir, "VarjoHMD", "VarjoHMD");
            TryAddConditionalModule(Target, EngineDir, "VarjoEyeTracker", "VarjoEyeTracker");
            // Windows Mixed Reality / HoloLens (built-in for UE 4.18+)
            TryAddConditionalModule(Target, EngineDir, "WindowsMixedRealityHMD", "WindowsMixedRealityHMD");
            TryAddConditionalModule(Target, EngineDir, "WindowsMixedRealityHandTracking", "WindowsMixedRealityHandTracking");
            // Common AR module
            TryAddConditionalModule(Target, EngineDir, "AugmentedReality", "AugmentedReality");
            // XRBase (core XR functionality)
            TryAddConditionalModule(Target, EngineDir, "XRBase", "XRBase");

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
                    Path.Combine(PluginsDir, "Runtime", "GoogleARCore", "Source", SearchName),
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
