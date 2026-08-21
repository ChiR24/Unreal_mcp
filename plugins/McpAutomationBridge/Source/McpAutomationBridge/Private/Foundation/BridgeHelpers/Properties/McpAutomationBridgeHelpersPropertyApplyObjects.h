#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * Import a UE export-text value into an already-resolved property value slot.
 *
 * Export text is the `(Key=Value,...)` form that ExportTextItem produces --
 * and it is exactly what `get_property` hands back to callers. Without an
 * import path for it, a caller could not feed our own output back into
 * `set_property`: reading `LayoutData` returned
 * `(Offsets=(Left=-48.000000,...))`, and passing that same string back failed
 * with PROPERTY_CONVERSION_FAILED. Accepting it closes that read/write
 * asymmetry.
 *
 * @param ValuePtr Pointer to the property's value (NOT the container).
 */
static inline bool ImportExportTextIntoValue(void *ValuePtr, FProperty *Property,
                                             const FString &Text) {
  if (!ValuePtr || !Property || Text.IsEmpty()) {
    return false;
  }

  // Parse into SCRATCH, not into the live property. ImportText writes struct members as it parses, so on
  // malformed input it returns nullptr having ALREADY written whatever it managed to read:
  // "(Offsets=(Left=10,Top=NOTANUMBER))" would leave Left=10 on the real object while the caller is told the
  // conversion failed. That is the same "the report disagrees with the object" class this batch exists to
  // remove, just inverted — and 0067 now turns it into a hard success:false, which would make the lie louder.
  // Honour the property's REQUIRED alignment: TArray<uint8> only guarantees the default allocator alignment,
  // and FProperty::GetMinAlignment() exists because that is not always enough. FTransform/FQuat/FVector4/
  // FMatrix are alignas(16), and InitializeValue/ImportText on an under-aligned buffer for an SSE type is
  // undefined behaviour. UE's binned allocator happens to return 16-aligned blocks for sizes >= 16, which is
  // exactly why getting this wrong would surface as a rare crash rather than a failing test.
  const int32 ScratchSize = Property->GetSize();
  const int32 ScratchAlign = Property->GetMinAlignment();
  void *Scratch = FMemory::Malloc(ScratchSize, ScratchAlign);
  if (!Scratch) {
    return false;
  }
  Property->InitializeValue(Scratch);
  ON_SCOPE_EXIT {
    Property->DestroyValue(Scratch);
    FMemory::Free(Scratch);
  };

  const TCHAR *Result = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Result = Property->ImportText_Direct(*Text, Scratch, nullptr, PPF_None, nullptr);
#else
  Result = Property->ImportText(*Text, Scratch, PPF_None, nullptr);
#endif
  if (!Result) {
    return false;
  }
  Property->CopyCompleteValue(ValuePtr, Scratch);
  return true;
}

static inline bool ApplyJsonObjectValueToProperty(void *TargetContainer, FProperty *Property,
                                                  const TSharedPtr<FJsonValue> &ValueField,
                                                  FString &OutError) {
  // Object reference
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      UObject *Res = nullptr;
      if (!Path.IsEmpty()) {
        // Try LoadObject first
        Res = LoadObject<UObject>(nullptr, *Path);
        // If unsuccessful, try finding by object path if it's a short path or
        // package path
        if (!Res && !Path.Contains(TEXT("."))) {
          // Fallback to StaticLoadObject which can sometimes handle vague paths
          // better
          Res = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
        }
      }
      if (!Res && !Path.IsEmpty()) {
        OutError =
            FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
        return false;
      }
      OP->SetObjectPropertyValue_InContainer(TargetContainer, Res);
      return true;
    }
    OutError = TEXT("Unsupported JSON type for object property");
    return false;
  }

  // Soft object references (FSoftObjectPtr)
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        if (Path.IsEmpty()) {
          *SoftObjPtr = FSoftObjectPtr();
        } else {
          *SoftObjPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft object property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        *SoftObjPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft object property requires string path or null");
    return false;
  }

  // Soft class references (FSoftClassPtr)
  if (FSoftClassProperty *SCP = CastField<FSoftClassProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        if (Path.IsEmpty()) {
          *SoftClassPtr = FSoftObjectPtr();
        } else {
          *SoftClassPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft class property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        *SoftClassPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft class property requires string path or null");
    return false;
  }

  // Structs (Vector/Rotator)
  if (FStructProperty *SP = CastField<FStructProperty>(Property)) {
    const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
    if (ValueField->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = ValueField->AsArray();
      if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FVector V((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                  (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &V);
        return true;
      }
      if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FRotator R((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                   (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &R);
        return true;
      }
    }

    // Try import from string for other structs. Prefer JSON conversion via
    // FJsonObjectConverter when the incoming text is valid JSON. Older
    // engine versions that provide ImportText on UScriptStruct are
    // supported via a guarded fallback for legacy builds.
		if (ValueField->Type == EJson::String) {
			const FString Txt = ValueField->AsString();
			if (SP->Struct) {
				// First attempt: parse the string as JSON and convert to struct
				// using the robust JsonObjectConverter which avoids relying on
				// engine-private textual import semantics.
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Txt);
				TSharedPtr<FJsonObject> ParsedObj;
				if (FJsonSerializer::Deserialize(Reader, ParsedObj) &&
					ParsedObj.IsValid()) {
					if (FJsonObjectConverter::JsonObjectToUStruct(
						ParsedObj.ToSharedRef(), SP->Struct,
						SP->ContainerPtrToValuePtr<void>(TargetContainer), 0, 0)) {
						return true;
					}
				}

				// The string was not JSON. Fall back to UE's own textual import,
				// which accepts the export-text form `get_property` emits. The
				// engine-revision differences that previously kept this out are
				// handled by the same ENGINE_MINOR_VERSION guard already used by
				// McpPropertyReflectionUtilities and the widget/AI handlers.
				if (ImportExportTextIntoValue(
						SP->ContainerPtrToValuePtr<void>(TargetContainer), SP, Txt)) {
					return true;
				}
			}
		}

		if (ValueField->Type == EJson::Object) {
			const TSharedPtr<FJsonObject> Object = ValueField->AsObject();
			if (Object.IsValid() && SP->Struct) {
				if (FJsonObjectConverter::JsonObjectToUStruct(
					Object.ToSharedRef(), SP->Struct,
					SP->ContainerPtrToValuePtr<void>(TargetContainer), 0, 0)) {
					return true;
				}
			}
		}

		OutError = FString::Printf(
				TEXT("Could not convert value to struct property of type '%s'. "
						 "Accepted forms: a JSON object ({\"Left\": -48, \"Top\": -64}), "
						 "UE export text ((Left=-48.000000,Top=-64.000000)) -- the form "
						 "get_property returns -- or, for Vector/Rotator, a 3-element "
						 "JSON array ([0, 0, 90])."),
				TypeName.IsEmpty() ? TEXT("<unknown>") : *TypeName);
    return false;
  }
  return false;
}
