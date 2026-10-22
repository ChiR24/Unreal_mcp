#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Foundation/BridgeHelpers/Properties/McpPropApply.h"

#if MCP_HAS_PCG_PARAMETERS
namespace McpPCGHandlers
{
namespace
{
bool ValidateScalar(EPropertyBagPropertyType Type, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    constexpr double MaxSafeInteger = 9007199254740991.0;
    if (Type == EPropertyBagPropertyType::Int64 && Value.IsValid() && Value->Type == EJson::Number &&
        FMath::Abs(Value->AsNumber()) > MaxSafeInteger)
    {
        OutError = TEXT("Int64 numbers outside the JSON safe-integer range must be supplied as decimal strings.");
        return false;
    }
    if (Type != EPropertyBagPropertyType::Bool) return true;
    if (Value.IsValid())
    {
        if (Value->Type == EJson::Boolean) return true;
        if (Value->Type == EJson::Number && (Value->AsNumber() == 0.0 || Value->AsNumber() == 1.0)) return true;
        if (Value->Type == EJson::String &&
            (Value->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value->AsString().Equals(TEXT("false"), ESearchCase::IgnoreCase))) return true;
    }
    OutError = TEXT("Bool parameters require a boolean, true/false string, or numeric zero/one.");
    return false;
}
}

bool StagePCGParameterValue(const FInstancedPropertyBag& Source, FName Name, const TSharedPtr<FJsonValue>& Value,
    FInstancedPropertyBag& OutBag, FString& OutError)
{
    FPCGReadBudget Budget;
    if (!Budget.CheckParameters(&Source))
    {
        OutError = TEXT("The source parameter values exceed the bounded authored-state budget.");
        return false;
    }
    OutBag = Source;
    const FPropertyBagPropertyDesc* Desc = OutBag.FindPropertyDescByName(Name);
    if (!Desc || !Desc->CachedProperty)
    {
        OutError = TEXT("The named graph parameter does not exist.");
        return false;
    }
    if (Desc->ValueType == EPropertyBagPropertyType::Text || Desc->ValueType == EPropertyBagPropertyType::UInt32 ||
        Desc->ValueType == EPropertyBagPropertyType::UInt64 ||
        (Desc->ValueType == EPropertyBagPropertyType::Struct && Desc->ValueTypeObject != TBaseStructure<FVector>::Get() &&
         Desc->ValueTypeObject != TBaseStructure<FRotator>::Get() && Desc->ValueTypeObject != TBaseStructure<FTransform>::Get()))
    {
        OutError = TEXT("This existing parameter type is not supported by the typed PCG setter.");
        return false;
    }
    if (Desc->ContainerTypes.Num() > 1 || (!Desc->ContainerTypes.IsEmpty() && Desc->ContainerTypes.GetFirstContainerType() != EPropertyBagContainerType::Array))
    {
        OutError = TEXT("Only scalar and single-array graph parameters are supported.");
        return false;
    }
    if (Value.IsValid() && Value->Type == EJson::Array && Value->AsArray().Num() > 10000)
    {
        OutError = TEXT("Parameter arrays are limited to 10000 elements per edit.");
        return false;
    }
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Desc->CachedProperty))
    {
        if (!Value.IsValid() || Value->Type != EJson::Array)
        {
            OutError = TEXT("An array parameter requires a JSON array value.");
            return false;
        }
        FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(OutBag.GetMutableValue().GetMemory()));
        Values.EmptyValues();
        Values.AddValues(Value->AsArray().Num());
        for (int32 Index = 0; Index < Value->AsArray().Num(); ++Index)
        {
            if (!ValidateScalar(Desc->ValueType, Value->AsArray()[Index], OutError)) return false;
            if (!::ApplyJsonValueToProperty(Values.GetRawPtr(Index), Array->Inner, Value->AsArray()[Index], OutError)) return false;
        }
        return true;
    }
    if (!ValidateScalar(Desc->ValueType, Value, OutError)) return false;
    return ::ApplyJsonValueToProperty(OutBag.GetMutableValue().GetMemory(), const_cast<FProperty*>(Desc->CachedProperty), Value, OutError);
}

bool CommitPCGParameterValue(UPCGGraphInterface* Target, FName Name, const FInstancedPropertyBag& Source, FString& OutError)
{
    const FPropertyBagPropertyDesc* Desc = Source.FindPropertyDescByName(Name);
    if (!Target || !Desc || !Desc->CachedProperty) return false;
    const FProperty* Property = Desc->CachedProperty;
    const void* Container = Source.GetValue().GetMemory();
    EPropertyBagResult Status = EPropertyBagResult::TypeMismatch;
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Values(Array, Array->ContainerPtrToValuePtr<void>(Container));
        const bool bChanged = Target->UpdateArrayGraphParameter(Name, [&Values, Array](FPropertyBagArrayRef& Destination)
        {
            Destination.EmptyValues();
            Destination.AddValues(Values.Num());
            for (int32 Index = 0; Index < Values.Num(); ++Index) Array->Inner->CopyCompleteValue(Destination.GetRawPtr(Index), Values.GetRawPtr(Index));
            return true;
        });
        if (bChanged) return true;
    }
    else if (Desc->ValueType == EPropertyBagPropertyType::Enum)
    {
        const FEnumProperty* Enum = CastField<FEnumProperty>(Property);
        const FNumericProperty* Numeric = Enum ? Enum->GetUnderlyingProperty() : CastField<FNumericProperty>(Property);
        const UEnum* Definition = Cast<UEnum>(Desc->ValueTypeObject);
        if (Numeric && Definition) Status = Target->SetGraphParameter(Name,
            Numeric->GetUnsignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Container)), Definition);
    }
    else if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property)) Status = Target->SetGraphParameter(Name, Bool->GetPropertyValue_InContainer(Container));
    else if (const FByteProperty* Byte = CastField<FByteProperty>(Property)) Status = Target->SetGraphParameter(Name, Byte->GetPropertyValue_InContainer(Container));
    else if (const FIntProperty* Int = CastField<FIntProperty>(Property)) Status = Target->SetGraphParameter(Name, Int->GetPropertyValue_InContainer(Container));
    else if (const FInt64Property* Integer64 = CastField<FInt64Property>(Property)) Status = Target->SetGraphParameter(Name, Integer64->GetPropertyValue_InContainer(Container));
    else if (const FFloatProperty* Float = CastField<FFloatProperty>(Property)) Status = Target->SetGraphParameter(Name, Float->GetPropertyValue_InContainer(Container));
    else if (const FDoubleProperty* Double = CastField<FDoubleProperty>(Property)) Status = Target->SetGraphParameter(Name, Double->GetPropertyValue_InContainer(Container));
    else if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property)) Status = Target->SetGraphParameter(Name, NameProperty->GetPropertyValue_InContainer(Container));
    else if (const FStrProperty* String = CastField<FStrProperty>(Property)) Status = Target->SetGraphParameter(Name, String->GetPropertyValue_InContainer(Container));
    else if (const FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPath Path = SoftObject->GetPropertyValue_InContainer(Container).ToSoftObjectPath();
        Status = Desc->ValueType == EPropertyBagPropertyType::SoftClass ? Target->SetGraphParameter(Name, TSoftClassPtr<UObject>(Path)) :
            Target->SetGraphParameter(Name, TSoftObjectPtr<UObject>(Path));
    }
    else if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property)) Status = Target->SetGraphParameter(Name, Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Container)));
    else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property)) Status = Target->SetGraphParameter(Name, ObjectProperty->GetObjectPropertyValue_InContainer(Container));
    else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (StructProperty->Struct == TBaseStructure<FVector>::Get()) Status = Target->SetGraphParameter(Name, *StructProperty->ContainerPtrToValuePtr<FVector>(Container));
        else if (StructProperty->Struct == TBaseStructure<FRotator>::Get()) Status = Target->SetGraphParameter(Name, *StructProperty->ContainerPtrToValuePtr<FRotator>(Container));
        else if (StructProperty->Struct == TBaseStructure<FTransform>::Get()) Status = Target->SetGraphParameter(Name, *StructProperty->ContainerPtrToValuePtr<FTransform>(Container));
    }
    if (Status == EPropertyBagResult::Success) return true;
    OutError = TEXT("The native graph parameter setter did not commit the staged value.");
    return false;
}
}
#endif
