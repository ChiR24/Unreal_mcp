#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// An optional engine module a feature needs (MassEntity, StateTree, NiagaraEditor, ...): true when it is loaded, or
// exists and loads now. A caller that may only test, never load, asks FModuleManager itself.
inline bool McpLoadOptionalModule(const TCHAR* Name)
{
    FModuleManager& Modules = FModuleManager::Get();
    return Modules.IsModuleLoaded(Name) || (Modules.ModuleExists(Name) && Modules.LoadModule(Name) != nullptr);
}
