#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

bool UMcpAutomationBridgeSubsystem::HandleMapSetValue(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("map_set_value"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("map_set")))
    return false;

  FString ObjectPath, PropertyName, Key;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("key"), Key)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires key."),
                        TEXT("INVALID_KEY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("map_set_value requires value field."),
                        TEXT("INVALID_VALUE"));
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
  FProperty *ValueProp = MapProp->ValueProp;
  void *TempKey =
      FMemory::Malloc(KeyProp->GetSize(), KeyProp->GetMinAlignment());
  void *TempValue =
      FMemory::Malloc(ValueProp->GetSize(), ValueProp->GetMinAlignment());
  KeyProp->InitializeValue(TempKey);
  ValueProp->InitializeValue(TempValue);

  bool bSuccess = false;
  if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
    *reinterpret_cast<FString *>(TempKey) = Key;
    bSuccess = true;
  } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
    *reinterpret_cast<FName *>(TempKey) = FName(*Key);
    bSuccess = true;
  } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
    *reinterpret_cast<int32 *>(TempKey) = FCString::Atoi(*Key);
    bSuccess = true;
  }

  if (!bSuccess) {
    KeyProp->DestroyValue(TempKey);
    ValueProp->DestroyValue(TempValue);
    FMemory::Free(TempKey);
    FMemory::Free(TempValue);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported map key type."),
                        TEXT("UNSUPPORTED_KEY_TYPE"));
    return true;
  }

  bSuccess = McpPropertyReflection::AssignPrimitiveFromJson(ValueProp, TempValue, ValueField);

  if (!bSuccess) {
    KeyProp->DestroyValue(TempKey);
    ValueProp->DestroyValue(TempValue);
    FMemory::Free(TempKey);
    FMemory::Free(TempValue);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported map value type."),
                        TEXT("UNSUPPORTED_VALUE_TYPE"));
    return true;
  }

  Helper.AddPair(TempKey, TempValue);

  KeyProp->DestroyValue(TempKey);
  ValueProp->DestroyValue(TempValue);
  FMemory::Free(TempKey);
  FMemory::Free(TempValue);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetStringField(TEXT("key"), Key);
  ResultPayload->SetNumberField(TEXT("mapSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Map value set."), ResultPayload, FString());
  return true;
}
