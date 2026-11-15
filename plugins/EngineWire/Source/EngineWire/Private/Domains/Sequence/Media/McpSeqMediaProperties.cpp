#include "Domains/Sequence/Media/McpSeqMedia.h"

// The media assets' members this folder writes by reflection: UMediaPlayer::Loop and the MediaPlayer of
// UMediaTexture and UMediaSoundComponent are protected, UPlatformMediaSource::MediaSource private; the public members
// written beside them go the same way. Functions are called directly.

// FStrProperty moved into its own header after 5.0; on 5.0 it is declared in
// UObject/UnrealType.h, included below.
#if __has_include("UObject/StrProperty.h")
#include "UObject/StrProperty.h"
#endif
#include "UObject/UnrealType.h"

namespace McpSequenceMedia {

bool SetObjectProperty(UObject *Object,
                       const FString &PropertyName,
                       UObject *Value) {
  FObjectPropertyBase *Property =
      Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(),
                                                  *PropertyName)
             : nullptr;
  if (!Property || (Value && !Value->IsA(Property->PropertyClass))) {
    return false;
  }
  Property->SetObjectPropertyValue_InContainer(Object, Value);
  return true;
}

bool SetBoolProperty(UObject *Object,
                     const FString &PropertyName,
                     bool Value) {
  FBoolProperty *Property =
      Object ? FindFProperty<FBoolProperty>(Object->GetClass(), *PropertyName)
             : nullptr;
  if (!Property) {
    return false;
  }
  Property->SetPropertyValue_InContainer(Object, Value);
  return true;
}

bool GetBoolProperty(const UObject *Object, const FString &PropertyName) {
  const FBoolProperty *Property =
      Object ? FindFProperty<FBoolProperty>(Object->GetClass(), *PropertyName)
             : nullptr;
  return Property && Property->GetPropertyValue_InContainer(Object);
}

bool SetStringProperty(UObject *Object,
                       const FString &PropertyName,
                       const FString &Value) {
  FStrProperty *Property =
      Object ? FindFProperty<FStrProperty>(Object->GetClass(), *PropertyName)
             : nullptr;
  if (!Property) {
    return false;
  }
  Property->SetPropertyValue_InContainer(Object, Value);
  return true;
}

bool SetStringObjectMapEntry(UObject *Object,
                             const FString &PropertyName,
                             const FString &Key,
                             UObject *Value) {
  FMapProperty *Map =
      Object ? FindFProperty<FMapProperty>(Object->GetClass(), *PropertyName)
             : nullptr;
  FStrProperty *KeyProperty =
      Map ? CastField<FStrProperty>(Map->KeyProp) : nullptr;
  FObjectPropertyBase *ValueProperty =
      Map ? CastField<FObjectPropertyBase>(Map->ValueProp) : nullptr;
  if (!Map || !KeyProperty || !ValueProperty || !Value ||
      !Value->IsA(ValueProperty->PropertyClass)) {
    return false;
  }
  FScriptMapHelper Helper(Map, Map->ContainerPtrToValuePtr<void>(Object));
  void *TempKey =
      FMemory::Malloc(KeyProperty->GetSize(), KeyProperty->GetMinAlignment());
  void *TempValue = FMemory::Malloc(ValueProperty->GetSize(),
                                    ValueProperty->GetMinAlignment());
  KeyProperty->InitializeValue(TempKey);
  ValueProperty->InitializeValue(TempValue);
  KeyProperty->SetPropertyValue(TempKey, Key);
  ValueProperty->SetObjectPropertyValue(TempValue, Value);
  Helper.AddPair(TempKey, TempValue);
  KeyProperty->DestroyValue(TempKey);
  ValueProperty->DestroyValue(TempValue);
  FMemory::Free(TempKey);
  FMemory::Free(TempValue);
  return true;
}

}
