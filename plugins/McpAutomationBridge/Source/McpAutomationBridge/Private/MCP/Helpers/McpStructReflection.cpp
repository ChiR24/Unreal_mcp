// McpStructReflection.cpp
#include "McpStructReflection.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "Engine/UserDefinedStruct.h"
#include "JsonObjectConverter.h"

namespace McpStructReflection
{
    FName ResolveFieldName(const UStruct* Struct, const FString& LogicalName)
    {
        if (!Struct) return NAME_None;

        // UUserDefinedStruct stores field names as "LogicalName_IDX_GUID"
        const UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Struct);
        if (UDS)
        {
            for (TFieldIterator<FProperty> It(UDS); It; ++It)
            {
                const FProperty* Prop = *It;
                const FString PropName = Prop->GetName();
                // Match by prefix up to the "_N_" pattern
                int32 UnderscoreIdx = INDEX_NONE;
                if (PropName.FindChar(TEXT('_'), UnderscoreIdx))
                {
                    const FString Logical = PropName.Left(UnderscoreIdx);
                    if (Logical.Equals(LogicalName, ESearchCase::IgnoreCase))
                    {
                        return Prop->GetFName();
                    }
                }
                // Fallback: exact match (for non-UDS or early-mangled)
                if (PropName.Equals(LogicalName, ESearchCase::IgnoreCase))
                {
                    return Prop->GetFName();
                }
            }
            return NAME_None;
        }

        // Native UStruct: direct FName lookup
        FProperty* Prop = Struct->FindPropertyByName(FName(*LogicalName));
        return Prop ? Prop->GetFName() : NAME_None;
    }

    bool SetStructFieldFromJson(
        const UStruct* Struct,
        void* StructInstance,
        FName FieldName,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError)
    {
        if (!Struct || !StructInstance) { OutError = TEXT("Null struct"); return false; }
        FProperty* Prop = Struct->FindPropertyByName(FieldName);
        if (!Prop)
        {
            OutError = FString::Printf(TEXT("Field not found: %s"), *FieldName.ToString());
            return false;
        }
        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
        if (!FJsonObjectConverter::JsonValueToUProperty(Value, Prop, ValuePtr, 0, CPF_Transient))
        {
            OutError = FString::Printf(TEXT("Failed to convert JSON to field %s"), *FieldName.ToString());
            return false;
        }
        return true;
    }

    bool SetStructFieldsFromJsonObject(
        const UStruct* Struct,
        void* StructInstance,
        const TSharedPtr<FJsonObject>& Fields,
        FString& OutError)
    {
        if (!Fields.IsValid()) { OutError = TEXT("Null fields object"); return false; }
        for (const auto& Pair : Fields->Values)
        {
            const FName ResolvedName = ResolveFieldName(Struct, Pair.Key);
            if (ResolvedName == NAME_None)
            {
                OutError = FString::Printf(TEXT("Unknown field: %s"), *Pair.Key);
                return false;
            }
            if (!SetStructFieldFromJson(Struct, StructInstance, ResolvedName, Pair.Value, OutError))
            {
                return false;
            }
        }
        return true;
    }

    TSharedPtr<FJsonObject> StructInstanceToJson(const UStruct* Struct, const void* StructInstance)
    {
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        if (!Struct || !StructInstance) return Out;
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            const FProperty* Prop = *It;
            const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
            TSharedPtr<FJsonValue> JsonVal = FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, CPF_Transient);

            // Strip UUserDefinedStruct GUID suffix for logical names
            FString Key = Prop->GetName();
            int32 UnderscoreIdx = INDEX_NONE;
            if (Struct->IsA<UUserDefinedStruct>() && Key.FindChar(TEXT('_'), UnderscoreIdx))
            {
                Key = Key.Left(UnderscoreIdx);
            }
            Out->SetField(Key, JsonVal);
        }
        return Out;
    }

    TSharedPtr<FJsonValue> GetStructFieldAsJson(
        const UStruct* Struct,
        const void* StructInstance,
        FName FieldName)
    {
        if (!Struct || !StructInstance) return nullptr;
        FProperty* Prop = Struct->FindPropertyByName(FieldName);
        if (!Prop) return nullptr;
        const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
        return FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, CPF_Transient);
    }
}
