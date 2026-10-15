#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "JsonObjectConverter.h"
#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * The component whose StaticMesh this property is, or null for any other property. That mesh goes through
 * SetStaticMesh: a raw write leaves KnownStaticMesh stale and the engine ensures on the next GetStaticMesh, and it
 * skips the render state an empty component needs and the physics, navigation, streaming and bounds updates. The
 * component class must own the property itself, so its container is the component: a struct member or a function
 * parameter named StaticMesh lives in another buffer (GetOwnerClass would walk up from a function to its class).
 */
static inline UStaticMeshComponent *McpStaticMeshOwner(const FProperty *Property, void *Container) {
  const UClass *Owner = Property ? Cast<UClass>(Property->GetOwnerUField()) : nullptr;
  return Owner && Owner->IsChildOf(UStaticMeshComponent::StaticClass()) &&
                 Property->GetFName() == FName(TEXT("StaticMesh"))
             ? static_cast<UStaticMeshComponent *>(Container)
             : nullptr;
}

/**
 * Fill a scratch value of Property and copy it over ValuePtr only when Fill returns true, so a refused write leaves
 * the property as it was. The scratch honours the property's REQUIRED alignment: TArray<uint8> only guarantees the
 * default allocator alignment, and FProperty::GetMinAlignment() exists because that is not always enough.
 * FTransform/FQuat/FVector4/FMatrix are alignas(16), and InitializeValue/ImportText on an under-aligned buffer for an
 * SSE type is undefined behaviour that UE's binned allocator would only surface as a rare crash.
 */
template <typename FillFn>
static inline bool McpWriteThroughScratch(void *ValuePtr, FProperty *Property, FillFn &&Fill) {
  void *Scratch = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
  if (!Scratch) {
    return false;
  }
  Property->InitializeValue(Scratch);
  ON_SCOPE_EXIT {
    Property->DestroyValue(Scratch);
    FMemory::Free(Scratch);
  };
  if (!Fill(Scratch)) {
    return false;
  }
  Property->CopyCompleteValue(ValuePtr, Scratch);
  return true;
}

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
  // conversion failed.
  return McpWriteThroughScratch(ValuePtr, Property, [Property, &Text](void *Scratch) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    // The error device stays GWarn: a null one crashed the editor on text an enum or struct import refused.
    return Property->ImportText_Direct(*Text, Scratch, nullptr, PPF_None) != nullptr;
#else
    return Property->ImportText(*Text, Scratch, PPF_None, nullptr) != nullptr;
#endif
  });
}

static inline bool ApplyJsonObjectValueToProperty(void *TargetContainer, FProperty *Property,
                                                  const TSharedPtr<FJsonValue> &ValueField,
                                                  FString &OutError) {
  // null, "" and "None" (how get_property reads an empty reference back) clear a reference.
  const bool bClearReference =
      ValueField->Type == EJson::Null ||
      (ValueField->Type == EJson::String &&
       (ValueField->AsString().IsEmpty() ||
        ValueField->AsString().Equals(TEXT("None"), ESearchCase::IgnoreCase)));

  // Object reference (a class reference is one too)
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    UStaticMeshComponent *MeshOwner = McpStaticMeshOwner(OP, TargetContainer);
    const auto Assign = [OP, TargetContainer, MeshOwner, &OutError](UObject *Value) {
      if (!MeshOwner) {
        OP->SetObjectPropertyValue_InContainer(TargetContainer, Value);
        return true;
      }
      MeshOwner->SetStaticMesh(Cast<UStaticMesh>(Value));
      const UStaticMesh *Now = MeshOwner->GetStaticMesh();
      if (Now == Value) {
        return true;
      }
      OutError = TEXT("The component kept its mesh: a static component's mesh cannot change once play has begun");
      return false;
    };
    if (bClearReference) {
      return Assign(nullptr);
    }
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      UObject *Res = LoadObject<UObject>(nullptr, *Path);
      // Fall back to StaticLoadObject for a short or package path.
      if (!Res && !Path.Contains(TEXT("."))) {
        Res = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
      }
      if (!Res) {
        OutError =
            FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
        return false;
      }
      // Stored as-is, an object of the wrong class crashes whatever reads it later.
      const FClassProperty *CP = CastField<FClassProperty>(Property);
      const UClass *Wanted = CP ? CP->MetaClass : OP->PropertyClass;
      const UClass *AsClass = Cast<UClass>(Res);
      if (CP ? !(AsClass && AsClass->IsChildOf(Wanted)) : !Res->IsA(Wanted)) {
        OutError = FString::Printf(
            TEXT("%s is a %s, but %s holds %s%s%s."), *Path,
            *Res->GetClass()->GetName(), *Property->GetName(),
            CP ? TEXT("a class derived from ") : TEXT("a "), *GetNameSafe(Wanted),
            CP && !AsClass ? TEXT(" (a Blueprint's class path ends in _C)") : TEXT(""));
        return false;
      }
      return Assign(Res);
    }
    OutError = TEXT("Object property requires a path string, or null / \"None\" to clear it");
    return false;
  }

  // Soft references; a soft class property is a soft object property, so this takes both.
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    FSoftObjectPtr *SoftPtr = SOP->ContainerPtrToValuePtr<FSoftObjectPtr>(TargetContainer);
    if (bClearReference) {
      *SoftPtr = FSoftObjectPtr();
      return true;
    }
    if (ValueField->Type == EJson::String) {
      *SoftPtr = FSoftObjectPath(ValueField->AsString());
      return true;
    }
    OutError = TEXT("Soft reference property requires a path string, or null / \"None\" to clear it");
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
			// 0-1 channels into an FColor, as the reflection importer reads them
			// (SCS templates took {R: 1, G: 0.82, B: 0.2} as a near-black 1,0,0).
			if (Object.IsValid() && SP->Struct == TBaseStructure<FColor>::Get() &&
				McpPropertyReflection::Private::TryImportNormalizedColor(
					Object, *SP->ContainerPtrToValuePtr<FColor>(TargetContainer))) {
				return true;
			}
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
