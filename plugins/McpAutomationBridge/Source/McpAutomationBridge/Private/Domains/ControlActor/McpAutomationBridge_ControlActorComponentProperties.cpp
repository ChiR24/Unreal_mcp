#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"

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
      if (McpHandlerUtils::TryGetJsonValueString(*MobilityVal, EnumVal)) {
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
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

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

    // A mesh asset has to go through the engine setter, never raw reflection.
    // UStaticMeshComponent::ShouldCreateRenderState() returns false while the
    // mesh is null, so a component that registered empty has NO render state at
    // all -- and MarkRenderStateDirty() only flags an existing one, so it does
    // nothing here. SetStaticMesh() is what notices that case and calls
    // RecreateRenderState_Concurrent(); it also runs the bookkeeping a raw write
    // skips (NotifyIfStaticMeshChanged, physics/nav/streaming state, bounds).
    // Writing the property directly leaves an actor that reports correct bounds
    // and a correct StaticMesh but is never drawn.
    if (PropertyName.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase)) {
      if (UStaticMeshComponent *MeshComp =
              Cast<UStaticMeshComponent>(TargetComponent)) {
        FString MeshPath;
        const bool bClearMesh = Pair.Value->Type == EJson::Null;
        if (bClearMesh || McpHandlerUtils::TryGetJsonValueString(Pair.Value, MeshPath)) {
          UStaticMesh *NewMesh = nullptr;
          if (!MeshPath.IsEmpty()) {
            NewMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (!NewMesh) {
              PropertyWarnings.Add(FString::Printf(
                  TEXT("Failed to set %s: no static mesh could be loaded from %s"),
                  *PropertyName, *MeshPath));
              continue;
            }
          }
          MeshComp->SetStaticMesh(NewMesh);
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

  // Some properties change whether the component renders at all; the helper
  // re-registers or rebuilds the render state accordingly.
  McpRefreshComponentAfterEdit(TargetComponent);
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  // Report whether the component ended up with a render state. Without this the
  // response cannot distinguish "property written" from "component will draw",
  // which is the exact gap that let a mesh assignment look successful while the
  // actor stayed invisible.
  if (Cast<UPrimitiveComponent>(TargetComponent)) {
    Data->SetBoolField(TEXT("renderStateCreated"),
                       TargetComponent->IsRenderStateCreated());
  }
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Data->SetArrayField(TEXT("applied"), PropsArray);
  }

  // PropertyWarnings was collected above and then thrown away: a value the
  // converter could not handle came back as "Component properties updated",
  // success:true, just with no `applied` entry -- so the caller had to diff the
  // property afterwards to discover nothing happened.
  if (PropertyWarnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warning : PropertyWarnings)
      WarnArray.Add(MakeShared<FJsonValueString>(Warning));
    Data->SetArrayField(TEXT("warnings"), WarnArray);
  }

	McpHandlerUtils::AddVerification(Data, Found);

  // Nothing asked for could be written: that is a failure, not an update.
  if (AppliedProperties.Num() == 0 && PropertyWarnings.Num() > 0) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No component properties were applied: %s"),
                        *FString::Join(PropertyWarnings, TEXT("; "))),
        Data, TEXT("PROPERTY_CONVERSION_FAILED"));
    return true;
  }

  // A partial apply is reported as a failure (PARTIAL_FAILURE) carrying the applied list (dogfood #151); it used to look identical to a clean one: success:true, the same message, and a `warnings`
  // array the model will not read. Put the shortfall where it cannot be missed.
  if (PropertyWarnings.Num() > 0) {
    Data->SetBoolField(TEXT("partial"), true);
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Applied %d of %d component properties (%d failed): %s"),
                        AppliedProperties.Num(),
                        AppliedProperties.Num() + PropertyWarnings.Num(),
                        PropertyWarnings.Num(),
                        *FString::Join(PropertyWarnings, TEXT("; "))),
        Data, TEXT("PARTIAL_FAILURE"));
    return true;
  }

	SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Data);
  return true;
#else
  return false;
#endif
}
