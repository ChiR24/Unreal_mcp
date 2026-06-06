#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

bool UMcpAutomationBridgeSubsystem::HandleArrayInsert(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("array_insert"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("array_insert")))
    return false;

  FString ObjectPath, PropertyName;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
      ObjectPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires objectPath."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) ||
      PropertyName.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires propertyName."),
                        TEXT("INVALID_PROPERTY"));
    return true;
  }

  int32 Index = -1;
  if (!Payload->TryGetNumberField(TEXT("index"), Index) || Index < 0) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires valid index."),
                        TEXT("INVALID_INDEX"));
    return true;
  }

  const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
  if (!ValueField.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("array_insert requires value field."),
                        TEXT("INVALID_VALUE"));
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
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to resolve property: %s"),
                          *ResolveError),
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
  if (Index > Helper.Num()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Index %d out of range (size: %d)"), Index,
                        Helper.Num()),
        TEXT("INDEX_OUT_OF_RANGE"));
    return true;
  }

  Helper.InsertValues(Index, 1);
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
        FString::Printf(TEXT("Failed to insert value: %s"), *ConversionError),
        TEXT("CONVERSION_FAILED"));
    return true;
  }

#if WITH_EDITOR
  RootObject->PostEditChange();
#endif

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
  ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
  ResultPayload->SetNumberField(TEXT("insertedAt"), Index);
  ResultPayload->SetNumberField(TEXT("newSize"), Helper.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Array element inserted."), ResultPayload,
                         FString());
  return true;
}
