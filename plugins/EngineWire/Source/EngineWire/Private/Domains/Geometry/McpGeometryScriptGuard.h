#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

// EngineWire links GeometryScriptingCore and GeometryScriptingEditor (delay-loaded on Win64, hard-linked on a Mac),
// yet its .uplugin names the GeometryScripting plugin only as Optional: the plugin is Experimental on UE 5.0-5.2, so
// the shared descriptor cannot require it, and an installed engine leaves an optional plugin off unless its receipt
// lists it (5.7's does not). A GeometryScript call then has no DLL to load on Windows, or runs a module whose plugin
// never started on a Mac. Every handler that calls GeometryScript asks this first and refuses with the text below.
inline bool McpGeometryScriptReady()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GeometryScripting"));
    if (!Plugin.IsValid() || !Plugin->IsEnabled())
    {
        return false;
    }
    FModuleManager& Modules = FModuleManager::Get();
    static const TCHAR* const Required[] = {TEXT("GeometryScriptingCore"), TEXT("GeometryScriptingEditor")};
    for (const TCHAR* Module : Required)
    {
        if (!Modules.IsModuleLoaded(Module) && !(Modules.ModuleExists(Module) && Modules.LoadModule(Module) != nullptr))
        {
            return false;
        }
    }
    return true;
}

inline constexpr const TCHAR* McpGeometryScriptMissingCode = TEXT("GEOMETRYSCRIPTING_PLUGIN_NOT_ENABLED");
inline constexpr const TCHAR* McpGeometryScriptMissing =
    TEXT("This action needs the GeometryScripting plugin (Geometry Script), which is not enabled or not loaded in this editor. "
         "Enable it in Edit > Plugins and restart the editor. Nothing was changed.");
