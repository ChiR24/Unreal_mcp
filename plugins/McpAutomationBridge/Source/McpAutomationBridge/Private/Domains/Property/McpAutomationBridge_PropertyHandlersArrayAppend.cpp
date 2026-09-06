#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

bool UMcpAutomationBridgeSubsystem::HandleArrayAppend(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_append"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_append")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ObjectPath;
  if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires objectPath."),
                        TEXT("INVALID_OBJECT"));
    return true;
  }

  FString PropertyName;
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_append requires value field."),
                        TEXT("INVALID_VALUE"));
    return true;
  }

  bool bObjectDenied = false;
  UObject *RootObject = McpSafeReflectionTarget::FindAddressableObject(ObjectPath, &bObjectDenied);
  if (!RootObject) {
    const FString NotFoundMessage = bObjectDenied
        ? FString(McpSafeReflectionTarget::DenyMessage())
        : FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath);
    SendAutomationError(
        RequestingSocket, RequestId,
        NotFoundMessage,
        bObjectDenied ? FString(McpSafeReflectionTarget::DenyCode()) : TEXT("OBJECT_NOT_FOUND"));
    return true;
  }

  void *TargetContainer = nullptr;
  FString ResolveError;
  FString ResolveErrorCode;
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
  const int32 NewIndex = Helper.AddValue();
  void *ElemPtr = Helper.GetRawPtr(NewIndex);
  FProperty *Inner = ArrayProp->Inner;

  FString ConversionError;
  if (!ApplyJsonValueToProperty(ElemPtr, Inner, ValueField,
                                ConversionError)) {
    bool bSuccess = McpPropertyReflection::AssignPrimitiveFromJson(Inner, ElemPtr, ValueField);

    if (!bSuccess) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to append value: %s"), *ConversionError),
          TEXT("CONVERSION_FAILED"));
      return true;
    }
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("newIndex"), NewIndex);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());

  if (AActor* AsActor = Cast<AActor>(RootObject)) {
    McpHandlerUtils::AddVerification(ResultPayload, AsActor);
  } else {
    McpHandlerUtils::AddVerification(ResultPayload, RootObject);
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element appended."), ResultPayload,
                         FString());
  return true;
}
