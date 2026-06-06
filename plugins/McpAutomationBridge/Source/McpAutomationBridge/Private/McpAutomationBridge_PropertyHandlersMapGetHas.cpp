#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleMapGetValue(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_get_value"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_get")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_get_value requires key."),
                        TEXT("INVALID_KEY"));
    return true;
  }

  UObject *RootObject = FindObject<UObject>(nullptr, *ObjectPath);
  if (!RootObject) {
    SendAutomationError(
        RequestingSocket, RequestId,
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
  FProperty *ValueProp = MapProp->ValueProp;

  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);
    FString KeyStr;

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
    }

    if (KeyStr.Equals(Key)) {
      const uint8 *ValuePtr = Helper.GetValuePtr(i);
      TSharedPtr<FJsonValue> ValueJson;

      if (FStrProperty *StrVal = CastField<FStrProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueString>(*reinterpret_cast<const FString *>(ValuePtr));
      } else if (FIntProperty *IntVal = CastField<FIntProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueNumber>((double)*reinterpret_cast<const int32 *>(ValuePtr));
      } else if (FFloatProperty *FloatVal = CastField<FFloatProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueNumber>((double)*reinterpret_cast<const float *>(ValuePtr));
      } else if (FBoolProperty *BoolVal = CastField<FBoolProperty>(ValueProp)) {
        ValueJson = MakeShared<FJsonValueBoolean>((*reinterpret_cast<const uint8 *>(ValuePtr)) != 0);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Unsupported map value type."),
                            TEXT("UNSUPPORTED_VALUE_TYPE"));
        return true;
      }

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetStringField(TEXT("key"), Key);
      ResultPayload->SetField(TEXT("value"), ValueJson);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Map value retrieved."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Key '%s' not found in map."), *Key),
                      TEXT("KEY_NOT_FOUND"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleMapHasKey(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_has_key"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_has")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_has_key requires key."), TEXT("INVALID_KEY"));
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
  bool bHasKey = false;
  for (int32 i = 0; i < Helper.Num(); ++i) {
    if (!Helper.IsValidIndex(i))
      continue;

    const uint8 *KeyPtr = Helper.GetKeyPtr(i);
    FString KeyStr;

    if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
      KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
    } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
      KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
    } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
      KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
    }

    if (KeyStr.Equals(Key)) {
      bHasKey = true;
      break;
    }
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetStringField(TEXT("key"), Key);
  ResultPayload->SetBoolField(TEXT("hasKey"), bHasKey);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bHasKey ? TEXT("Key exists in map.")
                                 : TEXT("Key does not exist in map."),
                         ResultPayload, FString());
  return true;
}
