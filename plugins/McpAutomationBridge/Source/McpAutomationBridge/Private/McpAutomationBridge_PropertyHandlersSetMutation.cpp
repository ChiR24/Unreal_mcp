#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleSetAdd(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_add"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_add")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_add requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
                        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to resolve property: %s"), *ResolveError),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;
  void *TempElem =
      FMemory::Malloc(ElemProp->GetSize(), ElemProp->GetMinAlignment());
  ElemProp->InitializeValue(TempElem);

  bool bSuccess = false;
  if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
    *reinterpret_cast<FString *>(TempElem) =
        (ValueField->Type == EJson::String)
            ? ValueField->AsString()
            : FString::Printf(TEXT("%g"), ValueField->AsNumber());
    bSuccess = true;
  } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
    *reinterpret_cast<int32 *>(TempElem) =
        (ValueField->Type == EJson::Number)
            ? (int32)ValueField->AsNumber()
            : FCString::Atoi(*ValueField->AsString());
    bSuccess = true;
  } else if (FFloatProperty *FloatElem = CastField<FFloatProperty>(ElemProp)) {
    *reinterpret_cast<float *>(TempElem) =
        (ValueField->Type == EJson::Number)
            ? (float)ValueField->AsNumber()
            : (float)FCString::Atod(*ValueField->AsString());
    bSuccess = true;
  } else if (FNameProperty *NameElem = CastField<FNameProperty>(ElemProp)) {
    *reinterpret_cast<FName *>(TempElem) = (ValueField->Type == EJson::String)
                                               ? FName(*ValueField->AsString())
                                               : NAME_None;
    bSuccess = true;
  }

  if (!bSuccess) {
    ElemProp->DestroyValue(TempElem);
    FMemory::Free(TempElem);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported set element type."),
                        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

  Helper.AddElement(TempElem);
  ElemProp->DestroyValue(TempElem);
  FMemory::Free(TempElem);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("setSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Element added to set."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetRemove(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_remove"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_remove")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_remove requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
                        TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FProperty *Property = nullptr;
  if (PropertyName.Contains(TEXT("."))) {
    FString ResolveError;
    Property = ResolveNestedPropertyPath(RootObject, PropertyName,
                                         TargetContainer, ResolveError);
    if (!Property || !TargetContainer) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Failed to resolve property: %s"), *ResolveError),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  } else {
    TargetContainer = RootObject;
    Property = RootObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Property not found."),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
  }

  FSetProperty *SetProp = CastField<FSetProperty>(Property);
  if (!SetProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a set."), TEXT("NOT_A_SET"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;

  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *ElemPtr = Helper.GetElementPtr(i);
    bool bMatch = false;

    if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
      const FString &ElemValue = *reinterpret_cast<const FString *>(ElemPtr);
      const FString SearchValue =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      bMatch = ElemValue.Equals(SearchValue);
    } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
      const int32 ElemValue = *reinterpret_cast<const int32 *>(ElemPtr);
      const int32 SearchValue = (ValueField->Type == EJson::Number)
                                    ? (int32)ValueField->AsNumber()
                                    : FCString::Atoi(*ValueField->AsString());
      bMatch = (ElemValue == SearchValue);
    } else if (FFloatProperty *FloatElem = CastField<FFloatProperty>(ElemProp)) {
      const float ElemValue = *reinterpret_cast<const float *>(ElemPtr);
      const float SearchValue =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      bMatch = FMath::IsNearlyEqual(ElemValue, SearchValue);
    }

    if (bMatch) {
      Helper.RemoveAt(i);

#if WITH_EDITOR
      RootObject->PostEditChange();
#endif

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetNumberField(TEXT("setSize"), Helper.Num());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Element removed from set."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Element not found in set."),
                      TEXT("ELEMENT_NOT_FOUND"));
  return true;
}
