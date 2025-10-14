#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

bool UMcpAutomationBridgeSubsystem::HandleSetObjectProperty(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString LowerAction = Action.ToLower();
    if (!Action.Equals(TEXT("set_object_property"), ESearchCase::IgnoreCase) && !LowerAction.Contains(TEXT("set_object_property"))) return false;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
        return true;
    }

    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
        return true;
    }

    const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
    if (!ValueField.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("set_object_property payload missing value field."), TEXT("INVALID_VALUE"));
        return true;
    }

    UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
    if (!TargetObject)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
        return true;
    }

    FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }

    FString ConversionError;
#if WITH_EDITOR
    TargetObject->Modify();
#endif

    if (!ApplyJsonValueToProperty(TargetObject, Property, ValueField, ConversionError))
    {
        SendAutomationError(RequestingSocket, RequestId, ConversionError, TEXT("PROPERTY_CONVERSION_FAILED"));
        return true;
    }

    bool bMarkDirty = true;
    if (Payload->HasField(TEXT("markDirty")))
    {
        if (!Payload->TryGetBoolField(TEXT("markDirty"), bMarkDirty)) { /* ignore parse failure, default true */ }
    }
    if (bMarkDirty) TargetObject->MarkPackageDirty();
#if WITH_EDITOR
    TargetObject->PostEditChange();
#endif

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
    ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
    ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);

    if (TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetObject, Property))
    {
        ResultPayload->SetField(TEXT("value"), CurrentValue);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value updated."), ResultPayload, FString());
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleGetObjectProperty(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString LowerAction = Action.ToLower();
    if (!Action.Equals(TEXT("get_object_property"), ESearchCase::IgnoreCase) && !LowerAction.Contains(TEXT("get_object_property"))) return false;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property payload missing."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || ObjectPath.TrimStartAndEnd().IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property requires a non-empty objectPath."), TEXT("INVALID_OBJECT"));
        return true;
    }

    FString PropertyName;
    if (!Payload->TryGetStringField(TEXT("propertyName"), PropertyName) || PropertyName.TrimStartAndEnd().IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("get_object_property requires a non-empty propertyName."), TEXT("INVALID_PROPERTY"));
        return true;
    }

    UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
    if (!TargetObject)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to find object at path %s."), *ObjectPath), TEXT("OBJECT_NOT_FOUND"));
        return true;
    }

    FProperty* Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Property %s not found on object %s."), *PropertyName, *ObjectPath), TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonValue> CurrentValue = ExportPropertyToJsonValue(TargetObject, Property);
    if (!CurrentValue.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unable to export property %s."), *PropertyName), TEXT("PROPERTY_EXPORT_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
    ResultPayload->SetStringField(TEXT("objectPath"), ObjectPath);
    ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
    ResultPayload->SetField(TEXT("value"), CurrentValue);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property value retrieved."), ResultPayload, FString());
    return true;
}
