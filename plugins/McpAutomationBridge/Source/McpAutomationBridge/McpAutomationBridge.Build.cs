using UnrealBuildTool;
using System;
using System.IO;

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
                "AssetTools",
                "MaterialEditor",
                // SubobjectData subsystem module (UE 5.6+)
                // NOTE: SubobjectData is an optional engine module introduced in UE 5.6.
                // Do not unconditionally add it here to avoid UBT failing on engine
                // versions that do not include this module. C++ source files already
                // guard inclusion of SubobjectData headers with __has_include so
                // the plugin can compile without it.
                });

            // Sequencer / LevelSequence editor modules (editor-only)
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LevelSequence",
                "LevelSequenceEditor",
                "Sequencer",
                "MovieScene",
                "MovieSceneTools",
                // Niagara support (editor + runtime where needed)
                "Niagara",
                "NiagaraEditor"
            });

            // LevelEditor: editor subsystem used by viewport / editor controls.
            // Add as an explicit private dependency so symbols like
            // ULevelEditorSubsystem::EditorInvalidateViewports are linked.
            PrivateDependencyModuleNames.Add("LevelEditor");

            // Optional SubobjectData module (UE 5.6+): only add when building
            // for an engine version that likely includes it so UBT will link
            // the subsystem implementation when present. We guard this by
            // engine version to avoid UBT errors on engine builds that do not
            // provide the module.
            // Optional SubobjectData module detection. Instead of relying solely
            // on engine version (which doesn't guarantee the module exists), try
            // to discover the engine root and check for the presence of a
            // SubobjectData header in common engine/plugin locations. Only add
            // the dependency when the header is found so UBT won't error when
            // the engine build omitted the optional SubobjectData module.
            string engineRoot = null;

            // Resolve an engine root via (1) env var override, (2) UBT-provided relative
            // engine path, or (3) scanning upward from the module directory for an
            // "Engine" folder. This is best-effort and must not throw.
            try
            {
                string envEngine = Environment.GetEnvironmentVariable("UE_ENGINE_DIRECTORY") ?? Environment.GetEnvironmentVariable("UE_ENGINE_ROOT");
                if (!string.IsNullOrEmpty(envEngine) && Directory.Exists(envEngine)) engineRoot = envEngine;

                if (engineRoot == null)
                {
                    try { var rel = Target.RelativeEnginePath; if (!string.IsNullOrEmpty(rel) && Directory.Exists(rel)) engineRoot = rel; } catch { }
                }

                if (engineRoot == null)
                {
                    // Try to locate the engine by walking up from the UnrealBuildTool assembly location.
                    // When UBT runs, its assemblies live under <Engine>/Binaries/DotNET/... which makes
                    // this a reliable way to locate the engine root for installed engines as well as
                    // several source builds.
                    try
                    {
                        var asmLocation = typeof(ReadOnlyTargetRules).Assembly.Location;
                        if (!string.IsNullOrEmpty(asmLocation))
                        {
                            var asmDir = new DirectoryInfo(Path.GetDirectoryName(asmLocation));
                            var dir = asmDir;
                            while (dir != null)
                            {
                                if (dir.Name.Equals("Engine", StringComparison.OrdinalIgnoreCase) && Directory.Exists(Path.Combine(dir.FullName, "Source")))
                                {
                                    engineRoot = dir.FullName;
                                    break;
                                }
                                dir = dir.Parent;
                            }
                        }
                    }
                    catch { /* best-effort: do not fail the build rule generation */ }

                    // As a final fallback, walk up from the module directory (project plugins) to
                    // discover the engine folder if present in the filesystem hierarchy.
                    if (engineRoot == null)
                    {
                        try
                        {
                            var dir = new DirectoryInfo(ModuleDirectory);
                            while (dir != null)
                            {
                                if (dir.Name.Equals("Engine", StringComparison.OrdinalIgnoreCase) && Directory.Exists(Path.Combine(dir.FullName, "Source")))
                                {
                                    engineRoot = dir.FullName;
                                    break;
                                }
                                dir = dir.Parent;
                            }
                        }
                        catch { }
                    }
                }
            }
            catch { /* proceed without engineRoot when unexpected errors occur */ }

            // Allow opt-out via environment var.
            var ignoreSubobject = Environment.GetEnvironmentVariable("MCP_IGNORE_SUBOBJECTDATA");
            if (!string.Equals(ignoreSubobject, "1", StringComparison.OrdinalIgnoreCase) && !string.Equals(ignoreSubobject, "true", StringComparison.OrdinalIgnoreCase))
            {
                bool mcpSubobjectMacroSet = false;

                // Prefer detection-based module addition rather than adding by engine
                // version. Relying solely on version can cause UBT to error on engine
                // builds where the optional SubobjectData module was omitted.
                if (!string.IsNullOrEmpty(engineRoot))
                {
                    bool foundHeader = false;
                    bool foundModuleDef = false;
                    // Known header names that indicate the presence of the SubobjectData module
                    string[] headerNames = new string[] {
                        "SubobjectDataSubsystem.h",
                        "SubobjectData.h",
                        "SubobjectDataHandle.h",
                        "SubobjectDataTypes.h",
                        "SubobjectDataModule.h"
                    };

                    foreach (var header in headerNames)
                    {
                        // Common engine and plugin locations
                        string[] candidates = new string[] {
                            Path.Combine(engineRoot, "Source", "Runtime", "SubobjectData", "Public", header),
                            Path.Combine(engineRoot, "Source", "Runtime", "SubobjectData", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "Runtime", "SubobjectData", "Source", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "Runtime", "SubobjectData", "Source", "SubobjectData", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "Enterprise", "SubobjectData", "Source", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "Enterprise", "SubobjectData", "Source", "SubobjectData", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "Marketplace", "SubobjectData", "Source", "Public", header),
                            Path.Combine(engineRoot, "Plugins", "SubobjectData", "Source", "Public", header)
                        };

                        foreach (var cand in candidates)
                        {
                            try { if (File.Exists(cand)) { foundHeader = true; break; } } catch { }
                        }

                        if (foundHeader) break;

                        // If not found in common locations, perform a limited scan of Source and Plugins directories
                        try
                        {
                            if (Directory.Exists(Path.Combine(engineRoot, "Source")))
                            {
                                foreach (var f in Directory.EnumerateFiles(Path.Combine(engineRoot, "Source"), header, SearchOption.AllDirectories)) { foundHeader = true; break; }
                            }
                        }
                        catch { /* ignore errors during enumeration */ }

                        if (foundHeader) break;

                        try
                        {
                            if (Directory.Exists(Path.Combine(engineRoot, "Plugins")))
                            {
                                foreach (var f in Directory.EnumerateFiles(Path.Combine(engineRoot, "Plugins"), header, SearchOption.AllDirectories)) { foundHeader = true; break; }
                            }
                        }
                        catch { /* ignore */ }

                        if (foundHeader) break;
                    }

                    // If a header was found, attempt to validate that a corresponding module
                    // definition (.Build.cs) or plugin (.uplugin) exists so we can safely
                    // add SubobjectData as a dependency. This prevents UBT errors when a
                    // header is present in the tree but no module is registered.
                    if (foundHeader)
                    {
                        try
                        {
                            var modulesToAdd = new System.Collections.Generic.List<string>();
                            // Check common Build.cs locations first
                            string[] explicitBuildCandidates = new string[] {
                                Path.Combine(engineRoot, "Source", "Runtime", "SubobjectData", "SubobjectData.Build.cs"),
                                Path.Combine(engineRoot, "Source", "Runtime", "SubobjectData", "Source", "SubobjectData", "SubobjectData.Build.cs"),
                                Path.Combine(engineRoot, "Plugins", "Runtime", "SubobjectData", "SubobjectData.Build.cs"),
                                Path.Combine(engineRoot, "Plugins", "Enterprise", "SubobjectData", "SubobjectData.Build.cs"),
                            };

                            foreach (var c in explicitBuildCandidates)
                            {
                                try
                                {
                                    if (File.Exists(c))
                                    {
                                        var fbase = Path.GetFileName(c); // e.g. SubobjectData.Build.cs
                                        var modname = fbase.Split('.')[0];
                                        modulesToAdd.Add(modname);
                                    }
                                }
                                catch { }
                            }

                            // Search engine Source and Plugins for any Build.cs whose filename contains 'SubobjectData'
                            try { if (Directory.Exists(Path.Combine(engineRoot, "Source"))) { foreach (var f in Directory.EnumerateFiles(Path.Combine(engineRoot, "Source"), "*SubobjectData*.Build.cs", SearchOption.AllDirectories)) { try { modulesToAdd.Add(Path.GetFileName(f).Split('.')[0]); } catch { } } } } catch { }
                            try { if (Directory.Exists(Path.Combine(engineRoot, "Plugins"))) { foreach (var f in Directory.EnumerateFiles(Path.Combine(engineRoot, "Plugins"), "*SubobjectData*.Build.cs", SearchOption.AllDirectories)) { try { modulesToAdd.Add(Path.GetFileName(f).Split('.')[0]); } catch { } } } } catch { }

                            // Also check for uplugin names containing SubobjectData and then look for Build.cs in that plugin
                            try
                            {
                                if (Directory.Exists(Path.Combine(engineRoot, "Plugins")))
                                {
                                    foreach (var p in Directory.EnumerateFiles(Path.Combine(engineRoot, "Plugins"), "*.uplugin", SearchOption.AllDirectories))
                                    {
                                        if (Path.GetFileNameWithoutExtension(p).IndexOf("SubobjectData", StringComparison.OrdinalIgnoreCase) >= 0)
                                        {
                                            var pluginDir = Path.GetDirectoryName(p);
                                            if (!string.IsNullOrEmpty(pluginDir) && Directory.Exists(pluginDir))
                                            {
                                                foreach (var b in Directory.EnumerateFiles(pluginDir, "*.Build.cs", SearchOption.AllDirectories)) { try { modulesToAdd.Add(Path.GetFileName(b).Split('.')[0]); } catch { } }
                                            }
                                        }
                                    }
                                }
                            }
                            catch { }

                            // Unique-ify module names and add them
                            var seen = new System.Collections.Generic.HashSet<string>(System.StringComparer.OrdinalIgnoreCase);
                            foreach (var mod in modulesToAdd)
                            {
                                if (string.IsNullOrEmpty(mod)) continue;
                                if (seen.Contains(mod)) continue;
                                seen.Add(mod);
                                try { if (!PrivateDependencyModuleNames.Contains(mod)) PrivateDependencyModuleNames.Add(mod); } catch { }
                            }

                            if (seen.Count > 0)
                            {
                                PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                                mcpSubobjectMacroSet = true;
                            }
                        }
                        catch { /* best-effort */ }
                    }
                    else
                    {
                        // engineRoot was empty — we'll look in project tree below and then
                        // set a default macro if we don't find a module there either.
                    }
                }
                // If we didn't detect SubobjectData under the engine root, also look in the
                // current project (module) tree. Some projects provide SubobjectData as a
                // project plugin rather than engine module.
                else
                {
                    try
                    {
                        var projDir = new DirectoryInfo(ModuleDirectory);
                        string projectRoot = null;
                        while (projDir != null)
                        {
                            if (projDir.GetFiles("*.uproject").Length > 0 || Directory.Exists(Path.Combine(projDir.FullName, "Content")))
                            {
                                projectRoot = projDir.FullName;
                                break;
                            }
                            projDir = projDir.Parent;
                        }

                        if (!string.IsNullOrEmpty(projectRoot))
                        {
                            bool foundProj = false;
                            string[] headerNames = new string[] { "SubobjectDataSubsystem.h", "SubobjectDataHandle.h", "SubobjectData.h" };
                            foreach (var header in headerNames)
                            {
                                try
                                {
                                    if (Directory.Exists(Path.Combine(projectRoot, "Plugins")))
                                    {
                                        foreach (var f in Directory.EnumerateFiles(Path.Combine(projectRoot, "Plugins"), header, SearchOption.AllDirectories)) { foundProj = true; break; }
                                    }
                                }
                                catch { }

                                if (foundProj)
                                {
                                    PrivateDependencyModuleNames.Add("SubobjectData");
                                    // Since we added the dependency for a project-provided module,
                                    // define the macro so the C++ code will compile SubobjectData paths.
                                    PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                                    mcpSubobjectMacroSet = true;
                                    break;
                                }
                            }
                        }
                    }
                    catch { }
                }
                    // Allow overriding detection: if a developer sets MCP_FORCE_SUBOBJECTDATA=1
                    // the build rule will attempt to add "SubobjectData" even when detection
                    // heuristics did not find a canonical module. This is useful for installed
                    // engine builds where module metadata may be located in non-standard
                    // locations. Use with caution; UBT will error if the forced module does
                    // not exist in the engine/project.
                    var forceSubobject = Environment.GetEnvironmentVariable("MCP_FORCE_SUBOBJECTDATA");
                    if (!string.IsNullOrEmpty(forceSubobject) && (string.Equals(forceSubobject, "1", StringComparison.OrdinalIgnoreCase) || string.Equals(forceSubobject, "true", StringComparison.OrdinalIgnoreCase)))
                    {
                        try
                        {
                            if (!PrivateDependencyModuleNames.Contains("SubobjectData")) PrivateDependencyModuleNames.Add("SubobjectData");
                            PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                            System.Console.WriteLine("McpAutomationBridge: Forced addition of SubobjectData via MCP_FORCE_SUBOBJECTDATA.");
                        }
                        catch { }
                    }
                // If detection did not find a module, conservatively attempt a version-based
                // fallback: on UE 5.6+ the SubobjectData module is expected to exist, so add
                // it and enable the C++ macro. If this is incorrect for a given engine
                // variant the developer can opt-out with MCP_IGNORE_SUBOBJECTDATA.
                if (!mcpSubobjectMacroSet)
                {
                    try
                    {
                        if (Target.Version != null)
                        {
                            int major = Target.Version.MajorVersion;
                            int minor = Target.Version.MinorVersion;
                            if (major > 5 || (major == 5 && minor >= 6))
                            {
                                if (!PrivateDependencyModuleNames.Contains("SubobjectData")) PrivateDependencyModuleNames.Add("SubobjectData");
                                PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                                mcpSubobjectMacroSet = true;
                                try { System.Console.WriteLine($"McpAutomationBridge: Added SubobjectData via version fallback (Target.Version={major}.{minor})."); } catch { }
                            }
                        }
                    }
                    catch { }

                    // If we still don't have it, set macro to 0 so C++ doesn't attempt to
                    // call SubobjectData APIs when they won't link.
                    if (!mcpSubobjectMacroSet)
                    {
                        PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=0");
                    }
                }
            }
    }
}
}
