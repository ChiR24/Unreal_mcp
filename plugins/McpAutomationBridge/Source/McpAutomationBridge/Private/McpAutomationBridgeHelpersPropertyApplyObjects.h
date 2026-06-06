#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

				// NOTE: ImportText-based struct parsing is intentionally omitted
				// because engine textual import signatures differ across engine
				// revisions and can produce fragile compilation failures. If a
				// non-JSON textual import format is required in the future we
				// can implement a safe parser here or add an explicit engine
				// compatibility shim guarded by a feature macro.
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

		OutError = TEXT("Unsupported JSON type for struct property");
    return false;
  }
  return false;
}
