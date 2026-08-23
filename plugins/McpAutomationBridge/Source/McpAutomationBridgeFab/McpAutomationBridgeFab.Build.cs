// Copyright (c) 2024 MCP Automation Bridge Contributors

using System;
using System.IO;
using UnrealBuildTool;

/**
 * The only module allowed to import Fab.dll or MegascansPlugin.dll.
 *
 * The core module used to take those dependencies directly. Because
 * PrivateDependencyModuleNames emits real DLL imports, an engine that shipped
 * Fab at build time but left it unmounted at runtime failed the entire plugin
 * with ERROR_MOD_NOT_FOUND, taking every unrelated MCP tool down with it. The
 * blast radius of a Fab problem is now this module alone.
 *
 * The plugin advertises UE 5.0-5.8 and Fab does not exist across that whole
 * range, so the dependencies stay probed rather than assumed: on an engine
 * without them this still compiles, still loads, and reports the capability as
 * unavailable instead of refusing to start.
 */
public class McpAutomationBridgeFab : ModuleRules
{
    public McpAutomationBridgeFab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Load-bearing, not an oversight: several files here define their own
        // EnsureCallback in an anonymous namespace, which a merged unity blob
        // rejects as a redefinition. At eight files unity would save nothing.
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "Json", "Projects", "McpAutomationBridge"
        });

        // Slate for walking the live tab hierarchy, WebBrowser for the public
        // ExecuteJavascript/BindUObject surface on the widget Fab already built.
        // All three are engine Runtime modules present across the advertised
        // 5.0-5.8 range, so unlike Fab itself they need no probing.
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate", "SlateCore", "WebBrowser"
        });

        if (Target.bBuildEditor)
        {
            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            bool bHasFab = AddOptionalModuleGroup(Target, EngineDir, "Fab", new string[] { "Fab" });
            PublicDefinitions.Add(bHasFab ? "MCP_FAB_ADAPTER_HAS_FAB=1" : "MCP_FAB_ADAPTER_HAS_FAB=0");

            bool bHasMegascans = AddOptionalModuleGroup(Target, EngineDir, "MegascansPlugin", new string[] { "MegascansPlugin" });
            PublicDefinitions.Add(bHasMegascans ? "MCP_FAB_ADAPTER_HAS_MEGASCANS=1" : "MCP_FAB_ADAPTER_HAS_MEGASCANS=0");
        }
        else
        {
            PublicDefinitions.AddRange(new string[] {
                "MCP_FAB_ADAPTER_HAS_FAB=0", "MCP_FAB_ADAPTER_HAS_MEGASCANS=0"
            });
        }
    }

    private bool ModuleExists(string EngineDir, string SearchName)
    {
        try
        {
            string PluginsDir = Path.Combine(EngineDir, "Plugins");
            if (!Directory.Exists(PluginsDir)) return false;
            return SearchDirectoryBounded(PluginsDir, SearchName, 3);
        }
        catch { return false; }
    }

    private bool SearchDirectoryBounded(string rootDir, string targetName, int maxDepth)
    {
        if (maxDepth < 0 || !Directory.Exists(rootDir)) return false;
        try
        {
            foreach (string subDir in Directory.GetDirectories(rootDir))
            {
                if (string.Equals(Path.GetFileName(subDir), targetName, StringComparison.OrdinalIgnoreCase)) return true;
                if (maxDepth > 0 && SearchDirectoryBounded(subDir, targetName, maxDepth - 1)) return true;
            }
        }
        catch { }
        return false;
    }

    private bool AddOptionalModuleGroup(ReadOnlyTargetRules Target, string EngineDir, string FeatureName, string[] ModuleNames)
    {
        foreach (string ModuleName in ModuleNames)
        {
            if (!ModuleExists(EngineDir, ModuleName))
            {
                Console.WriteLine(string.Format("McpAutomationBridgeFab: '{0}' disabled; no module '{1}'", FeatureName, ModuleName));
                return false;
            }
        }
        PrivateDependencyModuleNames.AddRange(ModuleNames);

        // Delay-loaded on purpose. A static import makes the loader resolve
        // Fab.dll before this module can run a single line, so an engine that
        // ships Fab but leaves it unmounted would fail the module and, with it,
        // the plugin. Delay-loading defers that to the first actual call, which
        // IsFabAvailable() gates behind a live IsModuleLoaded check.
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            foreach (string ModuleName in ModuleNames)
            {
                PublicDelayLoadDLLs.Add(string.Format("UnrealEditor-{0}.dll", ModuleName));
            }
        }
        Console.WriteLine(string.Format("McpAutomationBridgeFab: '{0}' enabled (delay-loaded)", FeatureName));
        return true;
    }
}
