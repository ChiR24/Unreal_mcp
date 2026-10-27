#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Include it rather than forward-declaring: relying on McpPropApply.h
// listing Objects before Arrays makes a build break out of swapping two include lines.
#include "Foundation/BridgeHelpers/Properties/McpPropApplyObjects.h"

// The elements of a JSON array into an empty array value, each through the importer every property uses (strict
// integer ranges, enum names), and an element given as text the JSON path cannot read through the engine's
// export-text import. Returns the refusal, or an empty string.
static inline FString McpFillArrayFromJson(FArrayProperty *AP, void *ArrayValue,
                                           const TArray<TSharedPtr<FJsonValue>> &Src) {
  FScriptArrayHelper Helper(AP, ArrayValue);
  for (int32 i = 0; i < Src.Num(); ++i) {
    void *ElemPtr = Helper.GetRawPtr(Helper.AddValue());
    FString Error;
    if (ApplyJsonValueToProperty(ElemPtr, AP->Inner, Src[i], Error) ||
        (Src[i]->Type == EJson::String && ImportExportTextIntoValue(ElemPtr, AP->Inner, Src[i]->AsString()))) {
      continue;
    }
    return FString::Printf(TEXT("Could not convert element %d of array property '%s' (element type '%s'): %s"), i,
                           *AP->GetName(), *AP->Inner->GetClass()->GetName(), *Error);
  }
  return FString();
}

static inline bool ApplyJsonArrayValueToProperty(void *TargetContainer, FProperty *Property,
                                                 const TSharedPtr<FJsonValue> &ValueField,
                                                 FString &OutError) {
  if (FArrayProperty *AP = CastField<FArrayProperty>(Property)) {
    if (ValueField->Type != EJson::Array) {
      // Callers commonly hand us an array that is *already* serialized: either
      // a double-encoded JSON array ("[1, 2, 3]") or UE export text
      // ("((Animation=\"...\",SampleValue=...))"), which is what get_property
      // returns for array properties. Accept both before failing.
      if (ValueField->Type == EJson::String) {
        const FString Txt = ValueField->AsString();

        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Txt);
        TArray<TSharedPtr<FJsonValue>> ParsedArray;
        if (FJsonSerializer::Deserialize(Reader, ParsedArray)) {
          const TSharedPtr<FJsonValue> Reparsed =
              MakeShared<FJsonValueArray>(ParsedArray);
          return ApplyJsonArrayValueToProperty(TargetContainer, Property,
                                               Reparsed, OutError);
        }

        if (ImportExportTextIntoValue(
                AP->ContainerPtrToValuePtr<void>(TargetContainer), AP, Txt)) {
          return true;
        }
      }

      OutError = FString::Printf(
          TEXT("Expected a JSON array for array property '%s'. Accepted forms: "
               "a JSON array ([1, 2, 3]), the same array as a JSON string "
               "(\"[1, 2, 3]\"), or UE export text ((A=1,B=2)) -- the form "
               "get_property returns."),
          *AP->GetName());
      return false;
    }
    const TArray<TSharedPtr<FJsonValue>> &Src = ValueField->AsArray();
    // Filled in scratch, so a refused element leaves the property as it was.
    return McpWriteThroughScratch(AP->ContainerPtrToValuePtr<void>(TargetContainer), AP, [AP, &Src, &OutError](void *Scratch) {
      OutError = McpFillArrayFromJson(AP, Scratch, Src);
      return OutError.IsEmpty();
    });
  }
  return false;
}

// The entries of a JSON object into an empty map value, each key and value through the importer every property uses.
// Returns the refusal, or an empty string. The map is hashed only once every entry is in, and a key that repeats an
// earlier one (two spellings of one enum value) is refused, since a map holds each key once.
static inline FString McpFillMapFromJson(FMapProperty *MP, void *MapValue, const TSharedPtr<FJsonObject> &Object) {
  FScriptMapHelper Helper(MP, MapValue);
  for (const auto &Entry : Object->Values) {
    const FString Key(Entry.Key.Len(), *Entry.Key);
    const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
    uint8 *Pair = Helper.GetPairPtr(Index);
    FString Error;
    if (ApplyJsonValueToProperty(Pair, MP->KeyProp, MakeShared<FJsonValueString>(Key), Error) &&
        ApplyJsonValueToProperty(Pair, MP->ValueProp, Entry.Value, Error)) {
      if (Helper.FindMapIndexWithKey(Pair) == Index) {
        continue;
      }
      Error = TEXT("an earlier key is the same key");
    }
    return FString::Printf(TEXT("Map property '%s', key '%s': %s"), *MP->GetName(), *Key, *Error);
  }
  Helper.Rehash();
  return FString();
}

// The same for a set from a JSON array.
static inline FString McpFillSetFromJson(FSetProperty *SP, void *SetValue, const TArray<TSharedPtr<FJsonValue>> &Src) {
  FScriptSetHelper Helper(SP, SetValue);
  for (int32 i = 0; i < Src.Num(); ++i) {
    const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
    uint8 *Element = Helper.GetElementPtr(Index);
    FString Error;
    if (ApplyJsonValueToProperty(Element, SP->ElementProp, Src[i], Error)) {
      if (Helper.FindElementIndex(Element) == Index) {
        continue;
      }
      Error = TEXT("an earlier element is the same value");
    }
    return FString::Printf(TEXT("Set property '%s', element %d: %s"), *SP->GetName(), i, *Error);
  }
  Helper.Rehash();
  return FString();
}

// A map takes a JSON object of key: value, the form get_property reads back (a JSON key is text, so a number or
// enum key is spelled as text); a set takes a JSON array. Filled in scratch, so a refused entry leaves the property
// as it was.
static inline bool ApplyJsonMapOrSetValueToProperty(void *TargetContainer, FProperty *Property,
                                                     const TSharedPtr<FJsonValue> &ValueField, FString &OutError) {
  FMapProperty *MP = CastField<FMapProperty>(Property);
  FSetProperty *SP = CastField<FSetProperty>(Property);
  if (!MP && !SP) {
    return false;
  }
  if (ValueField->Type != (MP ? EJson::Object : EJson::Array)) {
    OutError = MP ? FString::Printf(TEXT("Map property '%s' takes a JSON object of key: value, such as {\"Apple\": 3}"),
                                    *Property->GetName())
                  : FString::Printf(TEXT("Set property '%s' takes a JSON array, such as [\"Red\", \"Blue\"]"),
                                    *Property->GetName());
    return false;
  }
  return McpWriteThroughScratch(Property->ContainerPtrToValuePtr<void>(TargetContainer), Property,
                                [MP, SP, &ValueField, &OutError](void *Scratch) {
                                  OutError = MP ? McpFillMapFromJson(MP, Scratch, ValueField->AsObject())
                                                : McpFillSetFromJson(SP, Scratch, ValueField->AsArray());
                                  return OutError.IsEmpty();
                                });
}
