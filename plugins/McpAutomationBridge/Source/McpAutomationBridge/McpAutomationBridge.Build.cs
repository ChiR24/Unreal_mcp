using UnrealBuildTool;
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using EpicGames.Core;

public class McpAutomationBridge : ModuleRules {
    private const BindingFlags InstanceFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance;

    [StructLayout(LayoutKind.Sequential)]
    private struct MEMORYSTATUSEX {
        internal uint dwLength, dwMemoryLoad;
        internal ulong ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

    public McpAutomationBridge(ReadOnlyTargetRules Target) : base(Target) {
        long AvailableMemoryMB = GetActualAvailableMemoryMB();
        long TotalMemoryMB = GetTotalPhysicalMemoryMB();

        ApplyMsvcCompatibility(Target);
        Console.WriteLine(string.Format("McpAutomationBridge: Detected {0}MB available memory (of {1}MB total)", AvailableMemoryMB, TotalMemoryMB));

        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = true;
        TrySetIntMember(this, "NumIncludedBytesPerUnityCPPOverride", 256 * 1024);
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "JsonUtilities", "LevelSequence", "MovieScene", "MovieSceneTracks", "GameplayTags", "AIModule", "Landscape" });

        if (Target.bBuildEditor) {
            PublicDependencyModuleNames.AddRange(new string[] { "Sequencer", "MovieSceneTools", "Niagara", "UnrealEd", "WorldPartitionEditor", "DataLayerEditor", "MaterialEditor" });

            PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "Slate", "SlateCore", "Projects", "InputCore", "DeveloperSettings", "Settings", "EngineSettings", "Sockets", "Networking", "HTTP", "EditorSubsystem", "EditorScriptingUtilities", "BlueprintGraph", "ContentBrowser", "ContentBrowserData", "SSL", "Kismet", "KismetCompiler", "AssetRegistry", "AssetTools", "SourceControl", "AudioEditor", "AudioMixer", "PythonScriptPlugin" });

            AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

            PrivateDependencyModuleNames.AddRange(new string[] { "LandscapeEditor", "LandscapeEditorUtilities", "Foliage", "FoliageEdit", "AnimGraph", "AnimationBlueprintLibrary", "Persona", "ToolMenus", "EditorWidgets", "PropertyEditor", "LevelEditor", "RigVM", "RigVMDeveloper", "UMG", "UMGEditor", "MergeActors", "RenderCore", "RHI", "ImageWrapper", "AutomationController", "GameplayDebugger", "TraceLog", "TraceAnalysis", "AIGraph", "MeshUtilities", "MeshMergeUtilities", "MaterialUtilities", "PhysicsCore", "ClothingSystemRuntimeCommon", "GeometryCore", "GeometryFramework", "DynamicMesh", "MeshDescription", "StaticMeshDescription", "NavigationSystem" });

            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // UE 5.7+ relocated FContentBrowserItemPath (and its GetVirtualPathString accessor) into the
            // ContentBrowserData module. UBT treats ContentBrowser/ContentBrowserData as runtime-loaded editor
            // modules and does not add their .so to the link line, so concrete symbol references fail to link
            // under --unresolved-symbols=ignore-in-shared-libs. Force-link the prebuilt module binary when present
            // so those symbols resolve. No-op on engines/platforms where the module or its binary does not exist.
            if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
            {
                string CbdLibName = Target.Platform == UnrealTargetPlatform.Win64 ? "UnrealEditor-ContentBrowserData.lib" : "libUnrealEditor-ContentBrowserData.so";
                string CbdLibDir = Target.Platform == UnrealTargetPlatform.Win64 ? "Win64" : (Target.Platform == UnrealTargetPlatform.Mac ? "Mac" : "Linux");
                string CbdLib = Path.Combine(EngineDir, "Binaries", CbdLibDir, CbdLibName);
                if (File.Exists(CbdLib))
                {
                    PublicAdditionalLibraries.Add(CbdLib);
                    Console.WriteLine("McpAutomationBridge: force-linking " + CbdLib);
                }
            }

            AddOptionalModules(Target, EngineDir, new string[] { "D|GameplayAbilities|GameplayAbilities", "D|MetasoundEngine|MetasoundEngine", "C|MetasoundFrontend|MetasoundFrontend", "D|MetasoundEditor|MetasoundEditor", "D|StateTreeModule|StateTreeModule", "D|StateTreeEditorModule|StateTreeEditorModule", "D|SmartObjectsModule|SmartObjectsModule", "D|SmartObjectsEditorModule|SmartObjectsEditorModule", "C|StructUtils|StructUtils", "D|MassEntity|MassEntity", "D|MassSpawner|MassSpawner", "D|MassActors|MassActors", "D|OnlineSubsystem|OnlineSubsystem", "D|OnlineSubsystemUtils|OnlineSubsystemUtils", "D|ControlRig|ControlRig", "D|ControlRigDeveloper|ControlRigDeveloper", "D|ControlRigEditor|ControlRigEditor", "D|ProceduralMeshComponent|ProceduralMeshComponent", "D|EnvironmentQueryEditor|EnvironmentQueryEditor", "D|GeometryScriptingCore|GeometryScriptingCore", "D|GeometryScriptingEditor|GeometryScriptingEditor" });

            ProjectDescriptor Project = Target.ProjectFile == null ? null : ProjectDescriptor.FromFile(Target.ProjectFile);
            PluginDescriptor Bridge = PluginDescriptor.FromFile(new FileReference(Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "McpAutomationBridge.uplugin"))));
            bool bHasPCG = ((Project?.Plugins?.Any(Reference => string.Equals(Reference.Name, "PCG", StringComparison.OrdinalIgnoreCase) && Reference.bEnabled) ?? false) || (Bridge.Plugins?.Any(Reference => string.Equals(Reference.Name, "PCG", StringComparison.OrdinalIgnoreCase) && Reference.bEnabled && !Reference.bOptional) ?? false)) && AddOptionalDynamicModule(Target, EngineDir, "PCG", "PCG");
            PublicDefinitions.Add(bHasPCG ? "MCP_HAS_PCG=1" : "MCP_HAS_PCG=0");
            bool bHasCinematicCamera = AddOptionalModuleGroup(EngineDir, "CinematicCamera", new string[] { "CinematicCamera" });
            bool bHasMediaAssets = AddOptionalModuleGroup(EngineDir, "MediaAssets", new string[] { "MediaAssets" });
            bool bHasMovieRenderPipeline = AddOptionalModuleGroup(EngineDir, "Movie Render Pipeline", new string[] {
                "MovieRenderPipelineCore", "MovieRenderPipelineRenderPasses",
                "MovieRenderPipelineSettings", "MovieRenderPipelineEditor"
            });
            bool bHasMoviePipelineMaskModule = bHasMovieRenderPipeline &&
                AddOptionalModuleGroup(EngineDir, "Movie Pipeline Mask Render Pass", new string[] { "MoviePipelineMaskRenderPass" });
            bool bHasMoviePipelineObjectIdPass = bHasMoviePipelineMaskModule &&
                File.Exists(Path.Combine(EngineDir, "Plugins", "MovieScene", "MoviePipelineMaskRenderPass", "Source", "MoviePipelineMaskRenderPass", "Public", "MoviePipelineObjectIdPass.h"));
            bool bHasMoviePipelinePassMetadata = bHasMovieRenderPipeline &&
                FileContains(Path.Combine(EngineDir, "Plugins", "MovieScene", "MovieRenderPipeline", "Source", "MovieRenderPipelineRenderPasses", "Public", "MoviePipelineDeferredPasses.h"), "bHighPrecisionOutput");
            bool bHasSmaa = FileContains(Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public", "SceneUtils.h"), "AAM_SMAA");
            bool bHasTakeRecorder = AddOptionalModuleGroup(EngineDir, "Take Recorder", new string[] { "TakesCore", "TakeRecorder", "TakeRecorderSources" });
            bool bHasTakeRecorderOpenSequencer = bHasTakeRecorder &&
                FileContains(Path.Combine(EngineDir, "Plugins", "VirtualProduction", "Takes", "Source", "TakeRecorder", "Public", "Recorder", "TakeRecorderParameters.h"), "bOpenSequencer");
            bool bHasReplayApi = File.Exists(Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public", "ReplaySubsystem.h"));
            bool bHasReplaySubsystemTotalTime = bHasReplayApi &&
                FileContains(Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public", "ReplaySubsystem.h"), "GetReplayTotalTime");

            PublicDefinitions.Add(bHasCinematicCamera ? "MCP_HAS_CINEMATIC_CAMERA=1" : "MCP_HAS_CINEMATIC_CAMERA=0");
            PublicDefinitions.Add(bHasMediaAssets ? "MCP_HAS_MEDIA_ASSETS=1" : "MCP_HAS_MEDIA_ASSETS=0");
            PublicDefinitions.Add(bHasMovieRenderPipeline ? "MCP_HAS_MOVIE_RENDER_PIPELINE=1" : "MCP_HAS_MOVIE_RENDER_PIPELINE=0");
            PublicDefinitions.Add(bHasMoviePipelineObjectIdPass ? "MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS=1" : "MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS=0");
            PublicDefinitions.Add(bHasMoviePipelinePassMetadata ? "MCP_HAS_MOVIE_PIPELINE_PASS_METADATA=1" : "MCP_HAS_MOVIE_PIPELINE_PASS_METADATA=0");
            PublicDefinitions.Add(bHasSmaa ? "MCP_HAS_SMAA=1" : "MCP_HAS_SMAA=0");
            PublicDefinitions.Add(bHasTakeRecorder ? "MCP_HAS_TAKE_RECORDER=1" : "MCP_HAS_TAKE_RECORDER=0");
            PublicDefinitions.Add(bHasTakeRecorderOpenSequencer ? "MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER=1" : "MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER=0");
            PublicDefinitions.Add(bHasReplayApi ? "MCP_HAS_REPLAY_API=1" : "MCP_HAS_REPLAY_API=0");
            PublicDefinitions.Add(bHasReplaySubsystemTotalTime ? "MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME=1" : "MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME=0");

            AddOptionalModules(Target, EngineDir, new string[] { "D|LevelSequenceEditor|LevelSequenceEditor", "D|NiagaraEditor|NiagaraEditor", "D|EnhancedInput|EnhancedInput", "D|InputEditor|InputEditor", "D|BehaviorTreeEditor|BehaviorTreeEditor", "D|DataValidation|DataValidation", "D|Synthesis|Synthesis", "D|IKRig|IKRig", "D|ChaosVehicles|ChaosVehicles", "D|AnimationData|AnimationData" });

            PublicDefinitions.Add("MCP_HAS_K2NODE_HEADERS=1");
            PublicDefinitions.Add("MCP_HAS_EDGRAPH_SCHEMA_K2=1");
            ConfigureSubobjectData(EngineDir);
            PublicDefinitions.Add(HasWorldPartitionForEachDataLayer(EngineDir) ? "MCP_HAS_WP_FOR_EACH_DATALAYER=1" : "MCP_HAS_WP_FOR_EACH_DATALAYER=0");

            if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration == UnrealTargetConfiguration.Debug)
                PublicDefinitions.Add("MCP_ENABLE_EDIT_AND_CONTINUE=1");
        }
        else {
            PublicDefinitions.AddRange(new string[] { "MCP_HAS_K2NODE_HEADERS=0", "MCP_HAS_EDGRAPH_SCHEMA_K2=0", "MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=0", "MCP_HAS_WP_FOR_EACH_DATALAYER=0", "MCP_HAS_PCG=0", "MCP_HAS_CINEMATIC_CAMERA=0", "MCP_HAS_MEDIA_ASSETS=0", "MCP_HAS_MOVIE_RENDER_PIPELINE=0", "MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS=0", "MCP_HAS_MOVIE_PIPELINE_PASS_METADATA=0", "MCP_HAS_SMAA=0", "MCP_HAS_TAKE_RECORDER=0", "MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER=0", "MCP_HAS_REPLAY_API=0", "MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME=0" });
        }

        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 6)
            SetShadowVariableWarningLevel(WarningLevel.Warning);
    }

    private static void ApplyMsvcCompatibility(ReadOnlyTargetRules Target) {
        if (Target.Version.MajorVersion != 5 || Target.Version.MinorVersion > 2 || Target.Platform != UnrealTargetPlatform.Win64) return;

        try {
            var innerField = typeof(ReadOnlyTargetRules).GetField("Inner", InstanceFlags);
            TargetRules targetRules = innerField?.GetValue(Target) as TargetRules;
            if (targetRules == null) return;

            if (TrySetBooleanMember(targetRules, "bUndefinedIdentifierErrors", false, true)) {
                Console.WriteLine("McpAutomationBridge: Disabled bUndefinedIdentifierErrors for UE 5.0-5.2 MSVC build");
            }

            const string HasFeatureDefine = "__has_feature(x)=0";
            if (!targetRules.GlobalDefinitions.Contains(HasFeatureDefine)) {
                targetRules.GlobalDefinitions.Add(HasFeatureDefine);
                Console.WriteLine("McpAutomationBridge: Added __has_feature(x)=0 to GlobalDefinitions");
            }
        }
        catch (Exception Ex) {
            Console.WriteLine(string.Format("McpAutomationBridge: WARNING: Could not disable bUndefinedIdentifierErrors for UE 5.{0}: {1}", Target.Version.MinorVersion, Ex.Message));
        }

        Console.WriteLine(string.Format("McpAutomationBridge: Applied MSVC __has_feature compatibility for UE 5.{0}", Target.Version.MinorVersion));
    }

    private static bool TrySetBooleanMember(object target, string memberName, bool value, bool onlyIfCurrentlyTrue = false) =>
        TrySetMember(target, memberName, value, current => !onlyIfCurrentlyTrue || current);

    private static bool TrySetIntMember(object target, string memberName, int value) =>
        TrySetMember(target, memberName, value, _ => true);

    private static bool TrySetMember<T>(object target, string memberName, T value, Func<T, bool> canSet) {
        var property = target.GetType().GetProperty(memberName, InstanceFlags);
        if (property != null && property.PropertyType == typeof(T) && property.CanWrite) {
            T current = (T)property.GetValue(target);
            if (!canSet(current)) return false;
            property.SetValue(target, value);
            return true;
        }

        var field = target.GetType().GetField(memberName, InstanceFlags);
        if (field == null || field.FieldType != typeof(T) || !canSet((T)field.GetValue(target))) return false;
        field.SetValue(target, value);
        return true;
    }

    private void SetShadowVariableWarningLevel(WarningLevel level) {
        var cppSettings = GetType().GetProperty("CppCompileWarningSettings", InstanceFlags)?.GetValue(this);
        var shadowProperty = cppSettings?.GetType().GetProperty("ShadowVariableWarningLevel", InstanceFlags);
        if (shadowProperty != null && shadowProperty.CanWrite) {
            shadowProperty.SetValue(cppSettings, level); return;
        }

        var legacyProperty = GetType().GetProperty("ShadowVariableWarningLevel", InstanceFlags);
        if (legacyProperty != null && legacyProperty.CanWrite) legacyProperty.SetValue(this, level);
    }

    private static bool TryGetWindowsMemoryMB(out long availableMemoryMB, out long totalMemoryMB) {
        availableMemoryMB = 0;
        totalMemoryMB = 0;
        if (Environment.OSVersion.Platform != PlatformID.Win32NT) return false;

        var memStatus = new MEMORYSTATUSEX { dwLength = (uint)Marshal.SizeOf(typeof(MEMORYSTATUSEX)) };
        if (!GlobalMemoryStatusEx(ref memStatus)) return false;
        availableMemoryMB = (long)(memStatus.ullAvailPhys / (1024 * 1024));
        totalMemoryMB = (long)(memStatus.ullTotalPhys / (1024 * 1024));
        return true;
    }

    private static bool FileContains(string path, string text) {
        try { return File.Exists(path) && File.ReadAllText(path).Contains(text); }
        catch { return false; }
    }

    private static long GetActualAvailableMemoryMB() {
        try {
            long available, total;
            if (TryGetWindowsMemoryMB(out available, out total)) return available;
        }
        catch { }

        string memoryHint = Environment.GetEnvironmentVariable("UE_BUILD_MEMORY_MB");
        long hintValue;
        return !string.IsNullOrEmpty(memoryHint) && long.TryParse(memoryHint, out hintValue) && hintValue > 0 ? hintValue : 4096;
    }

    private static long GetTotalPhysicalMemoryMB() {
        try {
            long available, total;
            if (TryGetWindowsMemoryMB(out available, out total)) return total;
        }
        catch (Exception Ex) {
            Console.WriteLine(string.Format("McpAutomationBridge: Total memory detection failed: {0}", Ex.Message));
        }
        return 8192;
    }

    private void AddOptionalModules(ReadOnlyTargetRules Target, string EngineDir, string[] specs) {
        foreach (string spec in specs) {
            string[] parts = spec.Split('|');
            if (parts.Length == 3 && parts[0] == "D") AddOptionalDynamicModule(Target, EngineDir, parts[1], parts[2]);
            else if (parts.Length == 3 && parts[0] == "C") AddOptionalConditionalModule(EngineDir, parts[1], parts[2]);
        }
    }

    private bool FindOptionalModule(string EngineDir, string SearchName) {
        try {
            string[] directPaths = { Path.Combine(EngineDir, "Source", "Runtime", SearchName), Path.Combine(EngineDir, "Source", "Editor", SearchName) };
            foreach (string path in directPaths) if (Directory.Exists(path)) return true;

            string PluginsDir = Path.Combine(EngineDir, "Plugins");
            if (!Directory.Exists(PluginsDir)) return false;

            string OptionalEnginePluginDir = string.Concat("Exper", "imental");
            string[] pluginRoots = { "AI", "Runtime", OptionalEnginePluginDir, "Developer", "Animation", "Online" };
            foreach (string root in pluginRoots) if (Directory.Exists(Path.Combine(PluginsDir, root, SearchName))) return true;

            string[] pluginSourceRoots = { Path.Combine("Animation", "IKRig"), Path.Combine("Animation", "ControlRig"), Path.Combine("Runtime", "MassEntity"), Path.Combine("Runtime", "MassGameplay"), Path.Combine("Runtime", "SmartObjects"), Path.Combine("Runtime", "StateTree"), Path.Combine(OptionalEnginePluginDir, "ChaosVehiclesPlugin") };
            foreach (string root in pluginSourceRoots) if (Directory.Exists(Path.Combine(PluginsDir, root, "Source", SearchName))) return true;

            return SearchDirectoryBounded(PluginsDir, SearchName, 4);
        }
        catch { return false; }
    }

    private bool SearchDirectoryBounded(string rootDir, string targetName, int maxDepth) {
        if (maxDepth < 0 || !Directory.Exists(rootDir)) return false;
        try {
            foreach (string subDir in Directory.GetDirectories(rootDir)) {
                if (string.Equals(Path.GetFileName(subDir), targetName, StringComparison.OrdinalIgnoreCase)) return true;
                if (maxDepth > 0 && SearchDirectoryBounded(subDir, targetName, maxDepth - 1)) return true;
            }
        }
        catch { }
        return false;
    }

    private bool AddOptionalDynamicModule(ReadOnlyTargetRules Target, string EngineDir, string ModuleName, string SearchName) {
        if (!FindOptionalModule(EngineDir, SearchName)) return false;
        PrivateDependencyModuleNames.Add(ModuleName);
        if (Target.Platform == UnrealTargetPlatform.Win64) {
            PublicDelayLoadDLLs.Add(string.Format("UnrealEditor-{0}.dll", ModuleName));
        }
        Console.WriteLine(string.Format("McpAutomationBridge: Added optional module '{0}' with delay-load", ModuleName));
        return true;
    }

    private bool AddOptionalConditionalModule(string EngineDir, string ModuleName, string SearchName) {
        if (!FindOptionalModule(EngineDir, SearchName)) return false;
        PrivateDependencyModuleNames.Add(ModuleName);
        Console.WriteLine(string.Format("McpAutomationBridge: Added optional module '{0}' (conditional)", ModuleName));
        return true;
    }

    private bool AddOptionalModuleGroup(string EngineDir, string FeatureName, string[] ModuleNames) {
        string[] MissingModules = ModuleNames.Where(
            ModuleName => !FindOptionalModule(EngineDir, ModuleName)).ToArray();
        if (MissingModules.Length > 0) {
            Console.WriteLine(string.Format("McpAutomationBridge: Optional feature '{0}' disabled; missing modules: {1}",
                FeatureName, string.Join(", ", MissingModules)));
            return false;
        }

        PrivateDependencyModuleNames.AddRange(ModuleNames);
        Console.WriteLine(string.Format("McpAutomationBridge: Optional feature '{0}' enabled with modules: {1}",
            FeatureName, string.Join(", ", ModuleNames)));
        return true;
    }

    private void ConfigureSubobjectData(string EngineDir) {
        if (Directory.Exists(Path.Combine(EngineDir, "Source", "Editor", "SubobjectDataInterface"))) {
            PrivateDependencyModuleNames.Add("SubobjectDataInterface");
        }
        else if (!PrivateDependencyModuleNames.Contains("SubobjectData")) {
            PrivateDependencyModuleNames.Add("SubobjectData");
        }
        PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
    }

    private static bool HasWorldPartitionForEachDataLayer(string EngineDir) {
        try {
            string header = Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public",
                "WorldPartition", "DataLayer", "DataLayerManager.h");
            if (!File.Exists(header)) {
                header = Path.Combine(EngineDir, "Source", "Runtime", "Engine", "Public",
                    "WorldPartition", "WorldPartition.h");
            }
            return File.Exists(header) && File.ReadAllText(header).Contains("ForEachDataLayerInstance(");
        }
        catch (Exception Ex) {
            Console.WriteLine(string.Format("McpAutomationBridge: WorldPartition support detection failed: {0}", Ex.Message));
            return false;
        }
    }
}
