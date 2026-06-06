#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentName required"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> PropertiesObject;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && PropertiesPtr->IsValid()) {
    PropertiesObject = *PropertiesPtr;
  } else {
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    if (PropertyName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
    }
    const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
    if (PropertyName.IsEmpty() || !ValueField.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("properties object or propertyName/propertyPath and value required"), nullptr);
      return true;
    }
    PropertiesObject = MakeShared<FJsonObject>();
    PropertiesObject->SetField(PropertyName, ValueField);
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent *TargetComponent = FindComponentByName(Found, ComponentName);

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"), nullptr);
    return true;
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  UClass *ComponentClass = TargetComponent->GetClass();
  TargetComponent->Modify();

  // PRIORITY: Apply Mobility FIRST.
  // Physics simulation fails if the component is generic "Static".
  // Scan for Mobility key case-insensitively to ensure we find it regardless of
  // JSON casing
  const TSharedPtr<FJsonValue> *MobilityVal = nullptr;
  FString MobilityKey;
  for (const auto &Pair : PropertiesObject->Values) {
    const FString PropertyName(*Pair.Key);
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase)) {
      MobilityVal = &Pair.Value;
      MobilityKey = PropertyName;
      break;
    }
  }

  if (MobilityVal) {
    if (USceneComponent *SC = Cast<USceneComponent>(TargetComponent)) {
      FString EnumVal;
      if ((*MobilityVal)->TryGetString(EnumVal)) {
        // Parse enum string
        int64 Val =
            StaticEnum<EComponentMobility::Type>()->GetValueByNameString(
                EnumVal);
        if (Val != INDEX_NONE) {
          SC->SetMobility((EComponentMobility::Type)Val);
		AppliedProperties.Add(MobilityKey);
		}
	} else {
		double Val;
		if ((*MobilityVal)->TryGetNumber(Val)) {
			SC->SetMobility((EComponentMobility::Type)(int32)Val);
			AppliedProperties.Add(MobilityKey);
		}
      }
    }
  }

  for (const auto &Pair : PropertiesObject->Values) {
    const FString PropertyName(*Pair.Key);
    // Skip Mobility as we already handled it
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

    // Special handling for SimulatePhysics
    if (PropertyName.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase)) {
      if (UPrimitiveComponent *Prim =
              Cast<UPrimitiveComponent>(TargetComponent)) {
        bool bVal = false;
        if (Pair.Value->TryGetBool(bVal)) {
			Prim->SetSimulatePhysics(bVal);
				AppliedProperties.Add(PropertyName);
				continue;
        }
      }
    }

    FProperty *Property = ComponentClass->FindPropertyByName(*PropertyName);
    if (!Property) {
      PropertyWarnings.Add(
          FString::Printf(TEXT("Property not found: %s"), *PropertyName));
      continue;
    }
    FString ApplyError;
    if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value,
                                 ApplyError))
      AppliedProperties.Add(PropertyName);
    else
      PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                           *PropertyName, *ApplyError));
  }

  if (USceneComponent *SceneComponent =
          Cast<USceneComponent>(TargetComponent)) {
    SceneComponent->MarkRenderStateDirty();
    SceneComponent->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Data->SetArrayField(TEXT("applied"), PropsArray);
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Data);
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }

  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName, componentName, and propertyName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  // This handles cases where component names have numeric suffixes (e.g., "StaticMeshComponent0")
  UActorComponent* Component = FindComponentByName(Actor, ComponentName);
  if (!Component) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName),
        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Get property using reflection
  FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Property not found: %s on component: %s"), *PropertyName, *ComponentName),
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetStringField(TEXT("componentName"), ComponentName);
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());

  // Extract property value using the existing helper function
  TSharedPtr<FJsonValue> PropertyValue = ExportPropertyToJsonValue(Component, Property);
  if (PropertyValue.IsValid()) {
    Data->SetField(TEXT("value"), PropertyValue);
  } else {
    Data->SetStringField(TEXT("value"), TEXT("<unsupported property type>"));
  }

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
  return true;
#else
  return false;
#endif
}
