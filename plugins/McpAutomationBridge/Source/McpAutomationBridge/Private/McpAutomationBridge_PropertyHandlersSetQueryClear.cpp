#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleSetContains(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_contains"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_contains")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_contains requires value field."),
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

  FScriptSetHelper Helper(
      SetProp, SetProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *ElemProp = SetProp->ElementProp;
  bool bContains = false;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *ElemPtr = Helper.GetElementPtr(i);

    if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
      const FString &ElemValue = *reinterpret_cast<const FString *>(ElemPtr);
      const FString SearchValue =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      if (ElemValue.Equals(SearchValue)) {
        bContains = true;
        break;
      }
    } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
      const int32 ElemValue = *reinterpret_cast<const int32 *>(ElemPtr);
      const int32 SearchValue = (ValueField->Type == EJson::Number)
                                    ? (int32)ValueField->AsNumber()
                                    : FCString::Atoi(*ValueField->AsString());
      if (ElemValue == SearchValue) {
        bContains = true;
        break;
      }
    } else if (FFloatProperty *FloatElem = CastField<FFloatProperty>(ElemProp)) {
      const float ElemValue = *reinterpret_cast<const float *>(ElemPtr);
      const float SearchValue =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      if (FMath::IsNearlyEqual(ElemValue, SearchValue)) {
        bContains = true;
        break;
      }
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetBoolField(TEXT("contains"), bContains);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bContains ? TEXT("Element exists in set.")
                                   : TEXT("Element does not exist in set."),
                         ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("set_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("set_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_clear requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
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
  const int32 PrevSize = Helper.Num();
  Helper.EmptyElements();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Set cleared."), ResultPayload, FString());
  return true;
}
