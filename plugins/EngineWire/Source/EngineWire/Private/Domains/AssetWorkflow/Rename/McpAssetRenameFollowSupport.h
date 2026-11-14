// Copyright (c) 2025 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace McpAssetRename
{
// Re-keys one entry of a soft-path-keyed map, keeping its value.
inline void RekeySoftMapEntry(const FMapProperty* Map, void* Data, const FSoftObjectPath& From, const FSoftObjectPath& To)
{
    FScriptMapHelper Helper(Map, Data);
    FSoftObjectPtr FromKey(From);
    const int32 Index = Helper.FindMapIndexWithKey(&FromKey);
    if (Index == INDEX_NONE)
    {
        return;
    }
    const FProperty* ValueProp = Map->ValueProp;
    void* Value = FMemory::Malloc(ValueProp->GetSize(), ValueProp->GetMinAlignment());
    ValueProp->InitializeValue(Value);
    ValueProp->CopyCompleteValue(Value, Helper.GetValuePtr(Index));
    Helper.RemoveAt(Index);
    FSoftObjectPtr ToKey(To);
    Helper.AddPair(&ToKey, Value);
    ValueProp->DestroyValue(Value);
    FMemory::Free(Value);
}
}
