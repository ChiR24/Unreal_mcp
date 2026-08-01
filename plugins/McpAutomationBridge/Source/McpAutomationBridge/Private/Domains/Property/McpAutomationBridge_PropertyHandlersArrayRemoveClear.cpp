#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleArrayRemove(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_remove"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_remove")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_remove requires valid index."),
                        TEXT("INVALID_INDEX"));
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
  FString ResolveError, ResolveErrorCode;
  FProperty *Property = McpResolvePropertyContainer(
      RootObject, PropertyName, TargetContainer, ResolveError, ResolveErrorCode);
  if (!Property) {
    SendAutomationError(RequestingSocket, RequestId, ResolveError, ResolveErrorCode);
    return true;
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  if (Index >= Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  Helper.RemoveValues(Index, 1);

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("removedIndex"), Index);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());

  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element removed."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArrayClear(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_clear"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_clear")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_clear requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_clear requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
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
  FString ResolveError, ResolveErrorCode;
  FProperty *Property = McpResolvePropertyContainer(
      RootObject, PropertyName, TargetContainer, ResolveError, ResolveErrorCode);
  if (!Property) {
    SendAutomationError(RequestingSocket, RequestId, ResolveError, ResolveErrorCode);
    return true;
  }

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

#if WITH_EDITOR
  RootObject->Modify();
#endif

  FScriptArrayHelper Helper(
      ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetContainer));
  const int32 PrevSize = Helper.Num();
  Helper.EmptyValues();

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("previousSize"), PrevSize);
  ResultPayload->SetNumberField(TEXT("newSize"), 0);

  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array cleared."), ResultPayload, FString());
  return true;
}
