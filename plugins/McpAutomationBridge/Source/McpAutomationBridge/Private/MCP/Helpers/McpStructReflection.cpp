// McpStructReflection.cpp
#include "McpStructReflection.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "StructUtils/UserDefinedStruct.h"
#include "JsonObjectConverter.h"

namespace McpStructReflection
{
    FName ResolveFieldName(const UStruct* Struct, const FString& LogicalName)
    {
        if (!Struct) return NAME_None;

        // UUserDefinedStruct stores field names as "<Logical>_<Index>_<Hex32>"
        // where <Hex32> is a 32-char GUID. Logical names can themselves contain
        // underscores, so we must strip the suffix from the RIGHT (not split on
        // the first underscore).
        const UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Struct);
        if (UDS)
        {
            for (TFieldIterator<FProperty> It(UDS); It; ++It)
            {
                const FProperty* Prop = *It;
                const FString PropName = Prop->GetName();

                int32 LastUnderscore = INDEX_NONE;
                if (!PropName.FindLastChar(TEXT('_'), LastUnderscore))
                {
                    // No suffix; fall back to exact match.
                    if (PropName.Equals(LogicalName, ESearchCase::IgnoreCase))
                    {
                        return Prop->GetFName();
                    }
                    continue;
                }
                const FString AfterLastUnderscore = PropName.Mid(LastUnderscore + 1);
                // Expect 32 hex chars for the GUID; if not, treat as exact-match attempt.
                if (AfterLastUnderscore.Len() != 32)
                {
                    if (PropName.Equals(LogicalName, ESearchCase::IgnoreCase))
                    {
                        return Prop->GetFName();
                    }
                    continue;
                }
                const FString TrimOnce = PropName.Left(LastUnderscore);
                int32 SecondLast = INDEX_NONE;
                if (!TrimOnce.FindLastChar(TEXT('_'), SecondLast))
                {
                    continue;
                }
                const FString LogicalCandidate = TrimOnce.Left(SecondLast);
                if (LogicalCandidate.Equals(LogicalName, ESearchCase::IgnoreCase))
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
            FProperty* Prop = *It;
            const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
            TSharedPtr<FJsonValue> JsonVal = FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, CPF_Transient);

            // Strip UUserDefinedStruct GUID suffix for logical names.
            // Form is "<Logical>_<Index>_<Hex32>"; logical may contain underscores,
            // so strip from the RIGHT using the 32-char GUID as an anchor.
            FString Key = Prop->GetName();
            if (Cast<UUserDefinedStruct>(Struct) != nullptr)
            {
                int32 LastUnderscore = INDEX_NONE;
                if (Key.FindLastChar(TEXT('_'), LastUnderscore))
                {
                    const FString AfterLast = Key.Mid(LastUnderscore + 1);
                    if (AfterLast.Len() == 32)
                    {
                        const FString TrimOnce = Key.Left(LastUnderscore);
                        int32 SecondLast = INDEX_NONE;
                        if (TrimOnce.FindLastChar(TEXT('_'), SecondLast))
                        {
                            Key = TrimOnce.Left(SecondLast);
                        }
                    }
                }
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
