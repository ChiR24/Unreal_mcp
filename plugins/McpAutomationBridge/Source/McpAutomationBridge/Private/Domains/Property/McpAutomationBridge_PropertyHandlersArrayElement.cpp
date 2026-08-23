#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeReflectionTarget.h"

bool UMcpAutomationBridgeSubsystem::HandleArrayGetElement(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_get_element"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_get")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_get_element requires valid index."),
                        TEXT("INVALID_INDEX"));
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

  FArrayProperty *ArrayProp = CastField<FArrayProperty>(Property);
  if (!ArrayProp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Property is not an array."),
                        TEXT("NOT_AN_ARRAY"));
    return true;
  }

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

  void *ElemPtr = Helper.GetRawPtr(Index);
  FProperty *Inner = ArrayProp->Inner;

  TSharedPtr<FJsonValue> ElemValue = ExportPropertyToJsonValue(ElemPtr, Inner);
  if (!ElemValue.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Unsupported array element type."),
                        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("index"), Index);
  ResultPayload->SetField(TEXT("value"), ElemValue);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element retrieved."), ResultPayload,
                         FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleArraySetElement(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_set_element"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_set")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_set_element requires value field."),
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

  void *ElemPtr = Helper.GetRawPtr(Index);
  FProperty *Inner = ArrayProp->Inner;

  FString ConversionError;
  bool bSuccess = ApplyJsonValueToProperty(ElemPtr, Inner, ValueField,
                                           ConversionError);
  if (!bSuccess) {
    if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
      *reinterpret_cast<FString *>(ElemPtr) =
          (ValueField->Type == EJson::String)
              ? ValueField->AsString()
              : FString::Printf(TEXT("%g"), ValueField->AsNumber());
      bSuccess = true;
    } else if (FIntProperty *IntInner = CastField<FIntProperty>(Inner)) {
      *reinterpret_cast<int32 *>(ElemPtr) =
          (ValueField->Type == EJson::Number)
              ? (int32)ValueField->AsNumber()
              : FCString::Atoi(*ValueField->AsString());
      bSuccess = true;
    } else if (FFloatProperty *FloatInner = CastField<FFloatProperty>(Inner)) {
      *reinterpret_cast<float *>(ElemPtr) =
          (ValueField->Type == EJson::Number)
              ? (float)ValueField->AsNumber()
              : (float)FCString::Atod(*ValueField->AsString());
      bSuccess = true;
    } else if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
      *reinterpret_cast<uint8 *>(ElemPtr) =
          (ValueField->Type == EJson::Boolean)
              ? (ValueField->AsBool() ? 1 : 0)
              : (ValueField->AsNumber() != 0.0 ? 1 : 0);
      bSuccess = true;
    }
  }

  if (!bSuccess) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Failed to set array element: %s"),
                        *ConversionError),
        TEXT("UNSUPPORTED_TYPE"));
    return true;
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("index"), Index);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element updated."), ResultPayload,
                         FString());
  return true;
}
