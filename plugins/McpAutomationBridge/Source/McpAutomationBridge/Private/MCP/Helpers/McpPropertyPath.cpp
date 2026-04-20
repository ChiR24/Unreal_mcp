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

    // Walks the segment list and returns a direct pointer to the final property's value.
    // IMPORTANT: OutValuePtr is already offset-applied; callers MUST NOT call
    // ContainerPtrToValuePtr() on it again.
    static bool WalkToValue(
        UObject* Root,
        const TArray<FSegment>& Segments,
        FProperty*& OutFinalProp,
        void*& OutValuePtr,
        FString& OutError)
    {
        if (!Root) { OutError = TEXT("Null root"); return false; }
        if (Segments.Num() == 0) { OutError = TEXT("Empty path"); return false; }

        // CurrentContainer = pointer to a struct/object instance whose fields can be looked up
        //                    via CurrentStruct->FindPropertyByName(...).
        // CurrentProp      = the property that "produced" CurrentContainer when its target is a
        //                    container (array/map/set) waiting for an index segment; otherwise nullptr.
        void* CurrentContainer = Root;
        UStruct* CurrentStruct = Root->GetClass();
        FProperty* CurrentProp = nullptr;

        for (int32 i = 0; i < Segments.Num(); ++i)
        {
            const FSegment& Seg = Segments[i];
            const bool bIsLast = (i == Segments.Num() - 1);

            if (Seg.ArrayIndex >= 0)
            {
                // Previous prop must be FArrayProperty with CurrentContainer pointing at the
                // FScriptArray storage (i.e. the array property's value ptr).
                FArrayProperty* ArrayProp = CastField<FArrayProperty>(CurrentProp);
                if (!ArrayProp) { OutError = TEXT("Indexed segment on non-array"); return false; }
                FScriptArrayHelper Helper(ArrayProp, CurrentContainer);
                if (!Helper.IsValidIndex(Seg.ArrayIndex)) { OutError = FString::Printf(TEXT("Array index OOB: %d"), Seg.ArrayIndex); return false; }
                void* ElementPtr = Helper.GetRawPtr(Seg.ArrayIndex);

                if (bIsLast)
                {
                    OutFinalProp = ArrayProp->Inner;
                    OutValuePtr = ElementPtr;
                    return true;
                }

                // Descend into element for subsequent segments.
                if (FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
                {
                    CurrentContainer = ElementPtr;
                    CurrentStruct = InnerStruct->Struct;
                    CurrentProp = nullptr;
                }
                else if (FArrayProperty* InnerArray = CastField<FArrayProperty>(ArrayProp->Inner))
                {
                    // Nested array: element IS the FScriptArray storage; wait for next [N].
                    CurrentContainer = ElementPtr;
                    CurrentStruct = nullptr;
                    CurrentProp = InnerArray;
                }
                else
                {
                    OutError = FString::Printf(TEXT("Cannot descend into array element of type %s"), *ArrayProp->Inner->GetClass()->GetName());
                    return false;
                }
                continue;
            }

            // Name segment: look up on CurrentStruct.
            if (!CurrentStruct)
            {
                OutError = FString::Printf(TEXT("Cannot resolve field '%s' without a struct context"), *Seg.Name);
                return false;
            }
            FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Seg.Name));
            if (!Prop) { OutError = FString::Printf(TEXT("Field not found: %s"), *Seg.Name); return false; }

            void* FieldValuePtr = Prop->ContainerPtrToValuePtr<void>(CurrentContainer);

            if (bIsLast)
            {
                OutFinalProp = Prop;
                OutValuePtr = FieldValuePtr;
                return true;
            }

            // Descend for the next segment.
            if (FStructProperty* SP = CastField<FStructProperty>(Prop))
            {
                CurrentContainer = FieldValuePtr;
                CurrentStruct = SP->Struct;
                CurrentProp = nullptr;
            }
            else if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
            {
                // Next segment must be [N]; leave CurrentContainer at array storage.
                CurrentContainer = FieldValuePtr;
                CurrentStruct = nullptr;
                CurrentProp = AP;
            }
            else
            {
                OutError = FString::Printf(TEXT("Cannot descend into non-struct/non-array field: %s"), *Seg.Name);
                return false;
            }
        }
        OutError = TEXT("Walk terminated without producing a value");
        return false;
    }

    bool SetValueAtPath(UObject* Root, const FString& Path, const TSharedPtr<FJsonValue>& Value, FString& OutError)
    {
        TArray<FSegment> Segments;
        if (!ParsePath(Path, Segments, OutError)) return false;
        FProperty* FinalProp = nullptr;
        void* ValuePtr = nullptr;
        if (!WalkToValue(Root, Segments, FinalProp, ValuePtr, OutError)) return false;

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
        void* ValuePtr = nullptr;
        if (!WalkToValue(Root, Segments, FinalProp, ValuePtr, OutError)) return nullptr;
        return FJsonObjectConverter::UPropertyToJsonValue(FinalProp, ValuePtr, 0, CPF_Transient);
    }
}
