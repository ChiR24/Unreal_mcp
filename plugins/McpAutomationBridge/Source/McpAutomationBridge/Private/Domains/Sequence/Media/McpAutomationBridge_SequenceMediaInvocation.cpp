#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "UObject/StrProperty.h"
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

bool CallBoolFunction(UObject *Object, const FName &FunctionName) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    bool ReturnValue = false;
  } Params;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

bool CallBoolObjectFunction(UObject *Object,
                            const FName &FunctionName,
                            UObject *Argument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    UObject *Argument = nullptr;
    bool ReturnValue = false;
  } Params;
  Params.Argument = Argument;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

bool CallBoolStringFunction(UObject *Object,
                            const FName &FunctionName,
                            const FString &Argument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    FString Argument;
    bool ReturnValue = false;
  } Params;
  Params.Argument = Argument;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

bool CallBoolObjectIntFunction(UObject *Object,
                               const FName &FunctionName,
                               UObject *ObjectArgument,
                               int32 IntArgument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    UObject *ObjectArgument = nullptr;
    int32 IntArgument = 0;
    bool ReturnValue = false;
  } Params;
  Params.ObjectArgument = ObjectArgument;
  Params.IntArgument = IntArgument;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

UObject *CallObjectIntFunction(UObject *Object,
                               const FName &FunctionName,
                               int32 IntArgument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return nullptr;
  }
  struct {
    int32 IntArgument = 0;
    UObject *ReturnValue = nullptr;
  } Params;
  Params.IntArgument = IntArgument;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

bool CallBoolTimespanFunction(UObject *Object,
                              const FName &FunctionName,
                              const FTimespan &Argument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    FTimespan Argument;
    bool ReturnValue = false;
  } Params;
  Params.Argument = Argument;
  Object->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

bool CallVoidStringFunction(UObject *Object,
                            const FName &FunctionName,
                            const FString &Argument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return false;
  }
  struct {
    FString Argument;
  } Params;
  Params.Argument = Argument;
  Object->ProcessEvent(Function, &Params);
  return true;
}

void CallVoidObjectFunction(UObject *Object,
                            const FName &FunctionName,
                            UObject *Argument) {
  UFunction *Function = Object ? Object->FindFunction(FunctionName) : nullptr;
  if (!Function) {
    return;
  }
  struct {
    UObject *Argument = nullptr;
  } Params;
  Params.Argument = Argument;
  Object->ProcessEvent(Function, &Params);
}

void CallVoidFunction(UObject *Object, const FName &FunctionName) {
  if (UFunction *Function =
          Object ? Object->FindFunction(FunctionName) : nullptr) {
    Object->ProcessEvent(Function, nullptr);
  }
}

FString GetMediaUrl(UObject *MediaSource) {
  UFunction *Function =
      MediaSource ? MediaSource->FindFunction(TEXT("GetUrl")) : nullptr;
  if (!Function) {
    return FString();
  }
  struct {
    FString ReturnValue;
  } Params;
  MediaSource->ProcessEvent(Function, &Params);
  return Params.ReturnValue;
}

}
