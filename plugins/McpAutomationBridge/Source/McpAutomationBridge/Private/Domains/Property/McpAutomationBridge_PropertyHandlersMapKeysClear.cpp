#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

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

  bool bObjectDenied = false;
  UObject *RootObject = McpSafeReflectionTarget::FindAddressableObject(ObjectPath, &bObjectDenied);
  if (!RootObject) {
    const FString NotFoundMessage = bObjectDenied
        ? FString(McpSafeReflectionTarget::DenyMessage())
        : FString::Printf(TEXT("Object not found: %s"), *ObjectPath);
    SendAutomationError(
        RequestingSocket, RequestId,
        NotFoundMessage,
        bObjectDenied ? FString(McpSafeReflectionTarget::DenyCode()) : TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FString ResolveError, ResolveErrorCode;
  FProperty *Property = McpResolvePropertyContainer(
      RootObject, PropertyName, TargetContainer, ResolveError, ResolveErrorCode);
  if (!Property) {
    SendAutomationError(RequestingSocket, RequestId, ResolveError, ResolveErrorCode);
    return true;
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

  bool bObjectDenied = false;
  UObject *RootObject = McpSafeReflectionTarget::FindAddressableObject(ObjectPath, &bObjectDenied);
  if (!RootObject) {
    const FString NotFoundMessage = bObjectDenied
        ? FString(McpSafeReflectionTarget::DenyMessage())
        : FString::Printf(TEXT("Object not found: %s"), *ObjectPath);
    SendAutomationError(
        RequestingSocket, RequestId,
        NotFoundMessage,
        bObjectDenied ? FString(McpSafeReflectionTarget::DenyCode()) : TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FString ResolveError, ResolveErrorCode;
  FProperty *Property = McpResolvePropertyContainer(
      RootObject, PropertyName, TargetContainer, ResolveError, ResolveErrorCode);
  if (!Property) {
    SendAutomationError(RequestingSocket, RequestId, ResolveError, ResolveErrorCode);
    return true;
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
