#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleMapGetKeys(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_get_keys"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_get_keys")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_keys requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_keys requires propertyName."),
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

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  FProperty *KeyProp = MapProp->KeyProp;

  TArray<TSharedPtr<FJsonValue>> KeysArray;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueString>(*reinterpret_cast<const FString *>(KeyPtr)));
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueString>(reinterpret_cast<const FName *>(KeyPtr)->ToString()));
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeysArray.Add(MakeShared<FJsonValueNumber>((double)*reinterpret_cast<const int32 *>(KeyPtr)));
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetArrayField(TEXT("keys"), KeysArray);
  ResultPayload->SetNumberField(TEXT("keyCount"), KeysArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map keys retrieved."), ResultPayload, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_clear requires propertyName."),
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

  FMapProperty *MapProp = CastField<FMapProperty>(Property);
  if (!MapProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not a map."), TEXT("NOT_A_MAP"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptMapHelper Helper(
      MapProp, MapProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 PrevSize = Helper.Num();
  Helper.EmptyValues();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map cleared."), ResultPayload, FString());
  return true;
}
