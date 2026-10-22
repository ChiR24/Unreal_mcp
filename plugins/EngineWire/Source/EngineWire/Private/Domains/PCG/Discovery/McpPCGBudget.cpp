#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "UObject/UObjectHash.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool FPCGReadBudget::CheckValue(const FProperty* Property, void* Value, int32 Depth)
{
    if (!Property || Depth > 32 || ++Values > MaxValues || !IsWithinTime()) return false;
    if (const FStrProperty* String = CastField<FStrProperty>(Property)) Characters += String->GetPropertyValue(Value).Len();
    else if (const FTextProperty* Text = CastField<FTextProperty>(Property)) Characters += Text->GetPropertyValue(Value).ToString().Len();
    else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
        {
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
                if (!CheckValue(*It, It->ContainerPtrToValuePtr<void>(Value, Index), Depth + 1)) return false;
        }
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Items(Array, Value);
        if (Items.Num() > MaxValues - Values) return false;
        for (int32 Index = 0; Index < Items.Num(); ++Index)
            if (!CheckValue(Array->Inner, Items.GetRawPtr(Index), Depth + 1)) return false;
    }
    else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Items(Map, Value);
        if (Items.GetMaxIndex() > MaxValues - Values) return false;
        for (int32 Index = 0; Index < Items.GetMaxIndex(); ++Index)
        {
            if (Items.IsValidIndex(Index) && (!CheckValue(Map->KeyProp, Items.GetKeyPtr(Index), Depth + 1) ||
                !CheckValue(Map->ValueProp, Items.GetValuePtr(Index), Depth + 1))) return false;
        }
    }
    else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Items(Set, Value);
        if (Items.GetMaxIndex() > MaxValues - Values) return false;
        for (int32 Index = 0; Index < Items.GetMaxIndex(); ++Index)
            if (Items.IsValidIndex(Index) && !CheckValue(Set->ElementProp, Items.GetElementPtr(Index), Depth + 1)) return false;
    }
    return Characters <= MaxCharacters;
}

bool FPCGReadBudget::CheckObject(UObject* Object)
{
    if (!Object || ++Objects > MaxObjects) return false;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
    {
        if ((!It->HasAnyPropertyFlags(CPF_Edit) && !PCGRevisionExtraProperties().Contains(It->GetFName())) ||
            It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
        for (int32 Index = 0; Index < It->ArrayDim; ++Index)
            if (!CheckValue(*It, It->ContainerPtrToValuePtr<void>(Object, Index), 0)) return false;
    }
    return true;
}

bool FPCGReadBudget::CheckGraph(UPCGGraph* Graph)
{
    if (!Graph || Graph->GetNodes().Num() > MaxNodes - Nodes) return false;
    Nodes += Graph->GetNodes().Num();
    if (++VisitedObjects > MaxObjects || !IsWithinTime()) return false;
    bool bComplete = CheckObject(Graph);
    const auto Visit = [&](UObject* Object)
    {
        if (!bComplete) return false;
        if (++VisitedObjects > MaxObjects || !IsWithinTime())
        {
            bComplete = false;
            return false;
        }
        if (!IsPCGAuthoredObject(Object, Graph)) return true;
        bComplete = CheckObject(Object);
        if (const UPCGPin* Pin = Cast<UPCGPin>(Object))
        {
            if (Pin->IsOutputPin()) Edges += Pin->Edges.Num();
            bComplete &= Edges <= MaxEdges;
        }
        return bComplete;
    };
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    ForEachObjectWithOuterBreakable(Graph, Visit, true);
#else
    ForEachObjectWithOuter(Graph, [&](UObject* Object) { Visit(Object); }, true);
#endif
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    bComplete = bComplete && CheckParameters(Graph->GetUserParametersStruct());
#endif
    return bComplete && IsWithinTime();
}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
bool FPCGReadBudget::CheckParameters(const FInstancedPropertyBag* Bag)
{
    if (!Bag || !Bag->GetPropertyBagStruct()) return true;
    for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyBagStruct()->GetPropertyDescs())
    {
        if (!Desc.CachedProperty || !CheckValue(Desc.CachedProperty,
            Desc.CachedProperty->ContainerPtrToValuePtr<void>(const_cast<uint8*>(Bag->GetValue().GetMemory())), 0)) return false;
    }
    return true;
}
#endif
}
#endif
