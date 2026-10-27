#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "GameplayTagContainer.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

// Smart Objects are reached through reflection, never through SmartObjects' own headers: up to 5.2 those include
// MassEntityTypes.h, whose bitset templates every including file instantiates, and the instantiated code calls into
// MassEntity, a module this plugin does not link (a Mac build then fails to link).
namespace McpAIHandlers
{
// A SmartObjectsModule class by name (SmartObjectDefinition, SmartObjectComponent), loading the module when it is
// present but not loaded yet; nullptr when the SmartObjects plugin is not enabled.
inline UClass* FindSmartObjectClass(const TCHAR* ClassName)
{
    FModuleManager& Modules = FModuleManager::Get();
    if (!Modules.IsModuleLoaded(TEXT("SmartObjectsModule"))
        && (!Modules.ModuleExists(TEXT("SmartObjectsModule")) || !Modules.LoadModule(TEXT("SmartObjectsModule"))))
    {
        return nullptr;
    }
    return McpFindTypeQuiet(FString(TEXT("/Script/SmartObjectsModule.")) + ClassName);
}

// A Smart Object definition's Slots array, with the property of one slot in OutSlot; nullptr when it has none.
inline FArrayProperty* FindSmartObjectSlots(const UObject* Definition, FStructProperty*& OutSlot)
{
    FArrayProperty* Slots = Definition ? CastField<FArrayProperty>(Definition->GetClass()->FindPropertyByName(TEXT("Slots"))) : nullptr;
    OutSlot = Slots ? CastField<FStructProperty>(Slots->Inner) : nullptr;
    return OutSlot ? Slots : nullptr;
}
}
