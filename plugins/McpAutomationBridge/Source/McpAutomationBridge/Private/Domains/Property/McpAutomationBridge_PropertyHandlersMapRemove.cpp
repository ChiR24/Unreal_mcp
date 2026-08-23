#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

bool UMcpAutomationBridgeSubsystem::HandleMapRemoveKey(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_remove_key"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_remove")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_remove_key requires key."),
                        TEXT("INVALID_KEY"));
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
  FProperty *KeyProp = MapProp->KeyProp;

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
      Helper.RemoveAt(i);

#if WITH_EDITOR
      RootObject->PostEditChange();
#endif

      TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
      ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
      ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
      ResultPayload->SetStringField(TEXT("key"), Key);
      ResultPayload->SetNumberField(TEXT("mapSize"), Helper.Num());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Map key removed."), ResultPayload,
                             FString());
      return true;
    }
  }

  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Key '%s' not found in map."), *Key),
                      TEXT("KEY_NOT_FOUND"));
  return true;
}
