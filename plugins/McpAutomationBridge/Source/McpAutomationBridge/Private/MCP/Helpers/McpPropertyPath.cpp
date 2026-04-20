// McpPropertyPath.cpp
#include "McpPropertyPath.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"

namespace McpPropertyPath
{
    struct FSegment
    {
        FString Name;      // field name, empty if this is a pure array index
        int32 ArrayIndex;  // -1 if not an array index
    };

    static bool ParsePath(const FString& Path, TArray<FSegment>& OutSegments, FString& OutError)
    {
        TArray<FString> Parts;
        Path.ParseIntoArray(Parts, TEXT("."), true);
        for (const FString& P : Parts)
        {
            FSegment Seg;
            Seg.ArrayIndex = -1;
            if (P.StartsWith(TEXT("[")) && P.EndsWith(TEXT("]")))
            {
                const FString IndexStr = P.Mid(1, P.Len() - 2);
                if (!IndexStr.IsNumeric())
                {
                    OutError = FString::Printf(TEXT("Non-numeric array index: %s"), *P);
                    return false;
                }
                Seg.ArrayIndex = FCString::Atoi(*IndexStr);
            }
            else
            {
                Seg.Name = P;
            }
            OutSegments.Add(Seg);
        }
        return true;
    }

    static bool WalkToContainer(
        UObject* Root,
        const TArray<FSegment>& Segments,
        FProperty*& OutFinalProp,
        void*& OutFinalContainer,
        FString& OutError)
    {
        if (!Root) { OutError = TEXT("Null root"); return false; }
        void* CurrentContainer = Root;
        UStruct* CurrentStruct = Root->GetClass();
        FProperty* CurrentProp = nullptr;

        for (int32 i = 0; i < Segments.Num(); ++i)
        {
            const FSegment& Seg = Segments[i];
            const bool bIsLast = (i == Segments.Num() - 1);

            if (Seg.ArrayIndex >= 0)
            {
                // Previous prop must be FArrayProperty
                FArrayProperty* ArrayProp = CastField<FArrayProperty>(CurrentProp);
                if (!ArrayProp) { OutError = TEXT("Indexed segment on non-array"); return false; }
                FScriptArrayHelper Helper(ArrayProp, CurrentContainer);
                if (!Helper.IsValidIndex(Seg.ArrayIndex)) { OutError = FString::Printf(TEXT("Array index OOB: %d"), Seg.ArrayIndex); return false; }
                CurrentContainer = Helper.GetRawPtr(Seg.ArrayIndex);
                CurrentProp = ArrayProp->Inner;
                if (FStructProperty* InnerStruct = CastField<FStructProperty>(CurrentProp))
                {
                    CurrentStruct = InnerStruct->Struct;
                }
                if (bIsLast)
                {
                    OutFinalProp = CurrentProp;
                    OutFinalContainer = CurrentContainer;
                    return true;
                }
                continue;
            }

            // Name segment
            FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Seg.Name));
            if (!Prop) { OutError = FString::Printf(TEXT("Field not found: %s"), *Seg.Name); return false; }

            if (bIsLast)
            {
                OutFinalProp = Prop;
                OutFinalContainer = CurrentContainer;
                return true;
            }

            // Descend
            if (FStructProperty* SP = CastField<FStructProperty>(Prop))
            {
                CurrentContainer = SP->ContainerPtrToValuePtr<void>(CurrentContainer);
                CurrentStruct = SP->Struct;
                CurrentProp = Prop;
            }
            else if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
            {
                // Next segment must be [N]
                CurrentContainer = AP->ContainerPtrToValuePtr<void>(CurrentContainer);
                CurrentProp = Prop;
            }
            else
            {
                OutError = FString::Printf(TEXT("Cannot descend into non-struct/non-array field: %s"), *Seg.Name);
                return false;
            }
        }
        OutError = TEXT("Empty path");
        return false;
    }

    bool SetValueAtPath(UObject* Root, const FString& Path, const TSharedPtr<FJsonValue>& Value, FString& OutError)
    {
        TArray<FSegment> Segments;
        if (!ParsePath(Path, Segments, OutError)) return false;
        FProperty* FinalProp = nullptr;
        void* FinalContainer = nullptr;
        if (!WalkToContainer(Root, Segments, FinalProp, FinalContainer, OutError)) return false;
        void* ValuePtr = FinalProp->ContainerPtrToValuePtr<void>(FinalContainer);
        // For array element write, FinalContainer IS the raw element ptr (see WalkToContainer); skip Container offset
        if (Segments.Last().ArrayIndex >= 0) ValuePtr = FinalContainer;

        if (!FJsonObjectConverter::JsonValueToUProperty(Value, FinalProp, ValuePtr, 0, CPF_Transient))
        {
            OutError = TEXT("JsonValueToUProperty failed");
            return false;
        }
        Root->MarkPackageDirty();
        return true;
    }

    TSharedPtr<FJsonValue> GetValueAtPath(UObject* Root, const FString& Path, FString& OutError)
    {
        TArray<FSegment> Segments;
        if (!ParsePath(Path, Segments, OutError)) return nullptr;
        FProperty* FinalProp = nullptr;
        void* FinalContainer = nullptr;
        if (!WalkToContainer(Root, Segments, FinalProp, FinalContainer, OutError)) return nullptr;
        void* ValuePtr = FinalProp->ContainerPtrToValuePtr<void>(FinalContainer);
        if (Segments.Last().ArrayIndex >= 0) ValuePtr = FinalContainer;
        return FJsonObjectConverter::UPropertyToJsonValue(FinalProp, ValuePtr, 0, CPF_Transient);
    }
}
