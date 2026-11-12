#pragma once

#include "Interfaces/IPluginManager.h"

// This plugin as the plugin manager lists it: its .uplugin descriptor and its folder. The one lookup by name.
inline TSharedPtr<IPlugin> McpOwnPlugin()
{
	return IPluginManager::Get().FindPlugin(TEXT("EngineWire"));
}
