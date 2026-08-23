#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Include it rather than forward-declaring: relying on McpAutomationBridgeHelpersPropertyApply.h
// listing Objects before Arrays makes a build break out of swapping two include lines.
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyObjects.h"

static inline bool ApplyJsonArrayValueToProperty(void *TargetContainer, FProperty *Property,
                                                 const TSharedPtr<FJsonValue> &ValueField,
                                                 FString &OutError) {
  // Arrays: handle common inner element types directly. Unsupported inner
  // types will return an error to avoid relying on ImportText-like APIs.
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
    FScriptArrayHelper Helper(
        AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
    Helper.EmptyValues();
    const TArray<TSharedPtr<FJsonValue>> &Src = ValueField->AsArray();
    for (int32 i = 0; i < Src.Num(); ++i) {
      Helper.AddValue();
      void *ElemPtr = Helper.GetRawPtr(Helper.Num() - 1);
      FProperty *Inner = AP->Inner;
      const TSharedPtr<FJsonValue> &V = Src[i];
      if (FStrProperty *SIP = CastField<FStrProperty>(Inner)) {
        FString &Dest = *reinterpret_cast<FString *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? V->AsString()
                   : FString::Printf(TEXT("%g"), V->AsNumber());
        continue;
      }
      if (FNameProperty *NIP = CastField<FNameProperty>(Inner)) {
        FName &Dest = *reinterpret_cast<FName *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? FName(*V->AsString())
                   : FName(*FString::Printf(TEXT("%g"), V->AsNumber()));
        continue;
      }
      if (FBoolProperty *BIP = CastField<FBoolProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Boolean) ? (V->AsBool() ? 1 : 0)
                                           : (V->AsNumber() != 0.0 ? 1 : 0);
        continue;
      }
      if (FFloatProperty *FIP = CastField<FFloatProperty>(Inner)) {
        float &Dest = *reinterpret_cast<float *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (float)V->AsNumber()
                   : (float)FCString::Atod(*V->AsString());
        continue;
      }
      if (FDoubleProperty *DIP = CastField<FDoubleProperty>(Inner)) {
        double &Dest = *reinterpret_cast<double *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? V->AsNumber()
                                          : FCString::Atod(*V->AsString());
        continue;
      }
      if (FIntProperty *IIP = CastField<FIntProperty>(Inner)) {
        int32 &Dest = *reinterpret_cast<int32 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int32)V->AsNumber()
                                          : FCString::Atoi(*V->AsString());
        continue;
      }
      if (FInt64Property *I64IP = CastField<FInt64Property>(Inner)) {
        int64 &Dest = *reinterpret_cast<int64 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int64)V->AsNumber()
                                          : FCString::Atoi64(*V->AsString());
        continue;
      }
      if (FByteProperty *BYP = CastField<FByteProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (uint8)V->AsNumber()
                   : (uint8)FCString::Atoi(*V->AsString());
        continue;
      }

      FString InnerError;
      if (ApplyJsonValueToProperty(ElemPtr, Inner, V, InnerError)) {
        continue;
      }

      // Last resort for element types the JSON path cannot express: let the
      // engine parse the element's export text.
      if (V->Type == EJson::String &&
          ImportExportTextIntoValue(ElemPtr, Inner, V->AsString())) {
        continue;
      }

      // Still no: say which element failed, what it is, and what we accept --
      // "Unsupported array inner property type" told the caller nothing.
      OutError = FString::Printf(
          TEXT("Could not convert element %d of array property '%s' (element "
               "type '%s'%s). Directly supported element types: string, name, "
               "bool, float, double, int32, int64, byte, object reference, "
               "soft object/class reference, and struct (as a JSON object or "
               "UE export text)."),
          i, *AP->GetName(),
          Inner ? *Inner->GetClass()->GetName() : TEXT("<unknown>"),
          InnerError.IsEmpty() ? TEXT("")
                               : *FString::Printf(TEXT(": %s"), *InnerError));
      return false;
    }
    return true;
  }
  return false;
}
