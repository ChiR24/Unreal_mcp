using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class McpAutomationBridge : ModuleRules
{
    public McpAutomationBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core","CoreUObject","Engine","Json","JsonUtilities"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "ApplicationCore","Slate","SlateCore","Projects","InputCore","DeveloperSettings","Settings",
                "Sockets","Networking","UnrealEd","EditorSubsystem","EditorScriptingUtilities","BlueprintGraph",
                "Kismet","KismetCompiler","AssetRegistry","AssetTools","MaterialEditor","SourceControl",
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "LevelSequence","LevelSequenceEditor","Sequencer","MovieScene","MovieSceneTools","MovieSceneTracks",
                "Niagara","NiagaraEditor","Landscape","LandscapeEditor","LandscapeEditorUtilities","Foliage","FoliageEdit",
                "AnimGraph","AnimationBlueprintLibrary","Persona","ToolMenus","EditorWidgets","PropertyEditor","LevelEditor"
            });

            // Ensure editor builds expose full Blueprint graph editing APIs.
            PublicDefinitions.Add("MCP_HAS_K2NODE_HEADERS=1");
            PublicDefinitions.Add("MCP_HAS_EDGRAPH_SCHEMA_K2=1");

            // Optional SubobjectData: add only when present or explicitly forced
            bool addedSubobjectData = false;
            try
            {
                string force = Environment.GetEnvironmentVariable("MCP_FORCE_SUBOBJECTDATA") ?? string.Empty;
                string ignore = Environment.GetEnvironmentVariable("MCP_IGNORE_SUBOBJECTDATA") ?? string.Empty;
                if (!ignore.Equals("1", StringComparison.OrdinalIgnoreCase) && !ignore.Equals("true", StringComparison.OrdinalIgnoreCase))
                {
                    if (force.Equals("1", StringComparison.OrdinalIgnoreCase) || force.Equals("true", StringComparison.OrdinalIgnoreCase))
                    {
                        if (!PrivateDependencyModuleNames.Contains("SubobjectData"))
                        {
                            PrivateDependencyModuleNames.Add("SubobjectData");
                        }
                        PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                        addedSubobjectData = true;
                    }
                    else
                    {
                        // Detect engine root
                        string engineRoot = null;
                        try
                        {
                            string[] candidates = new string[]
                            {
                                Environment.GetEnvironmentVariable("UE_ENGINE_DIR"),
                                Environment.GetEnvironmentVariable("UE_ENGINE_DIRECTORY"),
                                Environment.GetEnvironmentVariable("UE_ENGINE_ROOT"),
                                Target.RelativeEnginePath
                            };
                            foreach (var cand in candidates)
                            {
                                if (!string.IsNullOrEmpty(cand) && Directory.Exists(cand)) { engineRoot = cand; break; }
                            }
                            if (engineRoot == null)
                            {
                                // Walk up from ModuleDirectory to find Engine folder
                                var dir = new DirectoryInfo(ModuleDirectory);
                                while (dir != null)
                                {
                                    if (dir.Name.Equals("Engine", StringComparison.OrdinalIgnoreCase) && Directory.Exists(Path.Combine(dir.FullName, "Source")))
                                    {
                                        engineRoot = dir.FullName; break;
                                    }
                                    dir = dir.Parent;
                                }
                            }
                        }
                        catch { }

                        Func<string, bool> HasSubobjectDataUnder = (root) =>
                        {
                            try
                            {
                                if (string.IsNullOrEmpty(root)) return false;
                                var runtimeDir = Path.Combine(root, "Source", "Runtime", "SubobjectData");
                                if (Directory.Exists(runtimeDir)) return true;
                                var header = Path.Combine(runtimeDir, "Public", "SubobjectDataSubsystem.h");
                                if (File.Exists(header)) return true;
                                var pluginsRuntime = Path.Combine(root, "Plugins", "Runtime");
                                if (Directory.Exists(pluginsRuntime))
                                {
                                    foreach (var d in Directory.EnumerateDirectories(pluginsRuntime, "SubobjectData", SearchOption.AllDirectories))
                                    {
                                        return true;
                                    }
                                }
                            }
                            catch { }
                            return false;
                        };

                        bool present = HasSubobjectDataUnder(engineRoot);

                        // Also check project Plugins
                        if (!present)
                        {
                            try
                            {
                                string projectRoot = null;
                                var dir = new DirectoryInfo(ModuleDirectory);
                                while (dir != null)
                                {
                                    if (dir.GetFiles("*.uproject").Length > 0) { projectRoot = dir.FullName; break; }
                                    dir = dir.Parent;
                                }
                                if (!string.IsNullOrEmpty(projectRoot))
                                {
                                    var projPlugins = Path.Combine(projectRoot, "Plugins");
                                    if (Directory.Exists(projPlugins))
                                    {
                                        foreach (var d in Directory.EnumerateDirectories(projPlugins, "SubobjectData", SearchOption.AllDirectories))
                                        {
                                            present = true; break;
                                        }
                                    }
                                }
                            }
                            catch { }
                        }

                        if (present)
                        {
                            if (!PrivateDependencyModuleNames.Contains("SubobjectData"))
                            {
                                PrivateDependencyModuleNames.Add("SubobjectData");
                            }
                            PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1");
                            addedSubobjectData = true;
                        }
                    }
                }
            }
            catch { }

            if (!addedSubobjectData)
            {
                PublicDefinitions.Add("MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=0");
            }

            // Ensure Win64 debug builds emit Edit-and-Continue friendly debug info
            if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration == UnrealTargetConfiguration.Debug)
            {
                PublicDefinitions.Add("MCP_ENABLE_EDIT_AND_CONTINUE=1");
            }
        }
        else
        {
            // Non-editor builds cannot rely on editor-only headers.
            PublicDefinitions.Add("MCP_HAS_K2NODE_HEADERS=0");
            PublicDefinitions.Add("MCP_HAS_EDGRAPH_SCHEMA_K2=0");
        }
    }
}
