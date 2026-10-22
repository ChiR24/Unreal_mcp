#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/BridgeHelpers/Security/McpSubobjectPath.h"

#if MCP_HAS_PCG_PARAMETERS
namespace McpPCGHandlers
{
namespace
{
TSharedPtr<FJsonValue> ExportPCGParameterValue(const void* Container, const FProperty* Property)
{
    if (const FInt64Property* Integer = CastField<FInt64Property>(Property))
    {
        constexpr int64 MaxSafeInteger = 9007199254740991LL;
        const int64 Value = Integer->GetPropertyValue_InContainer(Container);
        if (Value < -MaxSafeInteger || Value > MaxSafeInteger)
            return MakeShared<FJsonValueString>(LexToString(Value));
        return MakeShared<FJsonValueNumber>(static_cast<double>(Value));
    }
    const FArrayProperty* Array = CastField<FArrayProperty>(Property);
    if (Array && CastField<FInt64Property>(Array->Inner))
    {
        FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(Container));
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (int32 Index = 0; Index < Values.Num(); ++Index)
            Result.Add(ExportPCGParameterValue(Values.GetRawPtr(Index), Array->Inner));
        return MakeShared<FJsonValueArray>(Result);
    }
    return McpPropertyReflection::ExportPropertyToJsonValueOrText(const_cast<void*>(Container), const_cast<FProperty*>(Property));
}
}

bool ResolvePCGParameterTarget(const TSharedPtr<FJsonObject>& Payload, FPCGParameterTarget& Target, FString& OutError)
{
    Target.Scope = GetJsonStringField(Payload, TEXT("target"), TEXT("graph"));
    if ((Target.Scope != TEXT("graph") && Payload->HasField(TEXT("graphPath"))) ||
        (Target.Scope != TEXT("graphInstance") && Payload->HasField(TEXT("graphInstancePath"))) ||
        (Target.Scope != TEXT("component") && Payload->HasField(TEXT("componentPath"))))
    {
        OutError = TEXT("Supply only the asset/component path appropriate to the explicit parameter target.");
        return false;
    }
    FString Resolved;
    if (Target.Scope == TEXT("graph"))
    {
        Target.Interface = LoadPCGAsset<UPCGGraph>(GetJsonStringField(Payload, TEXT("graphPath")), TEXT("PCG graph"), Resolved, OutError);
    }
    else if (Target.Scope == TEXT("graphInstance"))
    {
        Target.Interface = LoadPCGAsset<UPCGGraphInstance>(GetJsonStringField(Payload, TEXT("graphInstancePath")), TEXT("PCG graph instance"), Resolved, OutError);
    }
    else if (Target.Scope == TEXT("component"))
    {
        UWorld* World = McpHandlerUtils::GetEditorWorld();
        AActor* Actor = nullptr;
        FString Selector, Package;
        if (!World || !McpSubobjectPath::TryCanonicalize(GetJsonStringField(Payload, TEXT("componentPath")), Selector, Package))
        {
            OutError = TEXT("Component parameters require the absolute componentPath returned by PCG component discovery or execution.");
            return false;
        }
        Target.Component = FindPCGComponent(World, FString(), Selector, Actor);
        if (!Target.Component || !Target.Component->GetPathName().Equals(Selector, ESearchCase::IgnoreCase))
        {
            OutError = TEXT("componentPath did not resolve exactly to a current editor-world PCG component.");
            return false;
        }
        Target.Interface = Target.Component->GetGraphInstance();
    }
    else
    {
        OutError = TEXT("Parameter target must be graph, graphInstance or component.");
        return false;
    }
    Target.Graph = Target.Interface ? Target.Interface->GetGraph() : nullptr;
    if (!Target.Interface || !Target.Graph || !Target.Interface->GetUserParametersStruct())
    {
        if (OutError.IsEmpty()) OutError = TEXT("Could not resolve the PCG parameter target or its parent graph.");
        return false;
    }
    if (Target.Scope != TEXT("graph") && Target.Interface->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        OutError = TEXT("Instance parameter edits cannot target a CDO or archetype.");
        return false;
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> ReadPCGParameters(const FPCGParameterTarget& Target, const FString& Name, int32 Offset, int32 Limit, int32* OutTotal)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (OutTotal) *OutTotal = 0;
    int32 Total = 0;
    const FInstancedPropertyBag* Bag = Target.Interface->GetUserParametersStruct();
    const FInstancedPropertyBag* Defaults = Target.Graph->GetUserParametersStruct();
    if (!Bag || !Bag->GetPropertyBagStruct()) return Result;
    const UPCGGraphInstance* Instance = Cast<UPCGGraphInstance>(Target.Interface);
    for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyBagStruct()->GetPropertyDescs())
    {
        if (!Name.IsEmpty() && Desc.Name.ToString() != Name) continue;
        if (Total++ < Offset || Result.Num() >= Limit) continue;
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Desc.Name.ToString());
        Row->SetStringField(TEXT("id"), Desc.ID.ToString(EGuidFormats::DigitsWithHyphens));
        Row->SetStringField(TEXT("type"), StaticEnum<EPropertyBagPropertyType>()->GetNameStringByValue(static_cast<int64>(Desc.ValueType)));
        Row->SetStringField(TEXT("container"), Desc.ContainerTypes.IsEmpty() ? TEXT("none") : Desc.ContainerTypes.GetFirstContainerType() == EPropertyBagContainerType::Array ? TEXT("array") : TEXT("unsupported"));
        if (Desc.ValueTypeObject) Row->SetStringField(TEXT("typeObject"), Desc.ValueTypeObject->GetPathName());
        if (Desc.CachedProperty)
        {
            Row->SetField(TEXT("value"), ExportPCGParameterValue(Bag->GetValue().GetMemory(), Desc.CachedProperty));
            Row->SetBoolField(TEXT("overridden"), Instance && Instance->IsPropertyOverridden(Desc.CachedProperty));
        }
        const FPropertyBagPropertyDesc* Default = Defaults ? Defaults->FindPropertyDescByID(Desc.ID) : nullptr;
        if (Default && Default->CachedProperty)
        {
            Row->SetField(TEXT("defaultValue"), ExportPCGParameterValue(Defaults->GetValue().GetMemory(), Default->CachedProperty));
        }
        Result.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (OutTotal) *OutTotal = Total;
    return Result;
}
}
#endif
