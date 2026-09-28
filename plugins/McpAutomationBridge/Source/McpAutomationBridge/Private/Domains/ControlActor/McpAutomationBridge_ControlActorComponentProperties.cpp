#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"

namespace {
// A mesh asset has to go through the engine setter, never raw reflection.
// UStaticMeshComponent::ShouldCreateRenderState() returns false while the mesh
// is null, so a component that registered empty has NO render state at all --
// and MarkRenderStateDirty() only flags an existing one, so it does nothing
// here. SetStaticMesh() is what notices that case and calls
// RecreateRenderState_Concurrent(); it also runs the bookkeeping a raw write
// skips (NotifyIfStaticMeshChanged, physics/nav/streaming state, bounds).
// Writing the property directly leaves an actor that reports correct bounds and
// a correct StaticMesh but is never drawn. Returns false when this is not a
// StaticMesh write on a StaticMeshComponent.
bool McpApplyComponentStaticMesh(UActorComponent *Component, const FString &Name,
                                 const TSharedPtr<FJsonValue> &Value, TArray<FString> &OutApplied,
                                 TArray<FString> &OutWarnings) {
  UStaticMeshComponent *MeshComp = Cast<UStaticMeshComponent>(Component);
  FString MeshPath;
  const bool bClearMesh = Value->Type == EJson::Null;
  if (!MeshComp || !Name.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase) ||
      !(bClearMesh || McpHandlerUtils::TryGetJsonValueString(Value, MeshPath))) {
    return false;
  }
  UStaticMesh *NewMesh = nullptr;
  if (!MeshPath.IsEmpty()) {
    NewMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!NewMesh) {
      OutWarnings.Add(FString::Printf(
          TEXT("Failed to set %s: no static mesh could be loaded from %s"), *Name, *MeshPath));
      return true;
    }
  }
  MeshComp->SetStaticMesh(NewMesh);
  OutApplied.Add(Name);
  return true;
}
} // namespace

void McpApplyComponentProperties(UActorComponent *Component, const TSharedPtr<FJsonObject> &Properties,
                                 TArray<FString> &OutApplied, TArray<FString> &OutWarnings) {
  // PRIORITY: Apply Mobility FIRST. Physics simulation fails on a Static
  // component, so the key is found case-insensitively before anything else.
  for (const auto &Pair : Properties->Values) {
    const FString PropertyName(*Pair.Key);
    if (!PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase)) {
      continue;
    }
    USceneComponent *SC = Cast<USceneComponent>(Component);
    FString EnumVal;
    double Number = 0.0;
    int64 Val = INDEX_NONE;
    if (McpHandlerUtils::TryGetJsonValueString(Pair.Value, EnumVal)) {
      Val = StaticEnum<EComponentMobility::Type>()->GetValueByNameString(EnumVal);
    } else if (Pair.Value->TryGetNumber(Number)) {
      Val = static_cast<int64>(Number);
    }
    // A value that did not land used to vanish without a warning.
    if (SC && Val >= EComponentMobility::Static && Val <= EComponentMobility::Movable) {
      SC->SetMobility(static_cast<EComponentMobility::Type>(Val));
      OutApplied.Add(PropertyName);
    } else {
      OutWarnings.Add(FString::Printf(
          TEXT("Failed to set %s: expects Static, Stationary or Movable on a scene component"), *PropertyName));
    }
    break;
  }

  bool bPhysicsChanged = false;
  for (const auto &Pair : Properties->Values) {
    const FString PropertyName(*Pair.Key);
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

    if (PropertyName.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase)) {
      UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Component);
      bool bVal = false;
      if (Prim && Pair.Value->TryGetBool(bVal)) {
        Prim->SetSimulatePhysics(bVal);
        OutApplied.Add(PropertyName);
        continue;
      }
    }

    if (McpApplyComponentStaticMesh(Component, PropertyName, Pair.Value, OutApplied, OutWarnings)) {
      continue;
    }

    FString ApplyError;
    if (McpIsCollisionSetterKey(Component, PropertyName)) {
      if (McpApplyCollisionSetterKey(Component, PropertyName, Pair.Value, ApplyError))
        OutApplied.Add(PropertyName);
      else
        OutWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *PropertyName, *ApplyError));
      continue;
    }

    // The shared resolver the read path uses: dotted struct paths at any depth
    // (BodyInstance.bNotifyRigidBodyCollision) and bare nested member names.
    void *Container = nullptr;
    FString ResolvedPath;
    FProperty *Property = McpResolvePropertyPath(Component, PropertyName, Container, ResolvedPath, ApplyError);
    if (!Property || !Container) {
      OutWarnings.Add(FString::Printf(TEXT("Property not found: %s (%s)"), *PropertyName, *ApplyError));
      continue;
    }
    if (ApplyJsonValueToProperty(Container, Property, Pair.Value, ApplyError)) {
      OutApplied.Add(PropertyName);
      // A raw write into BodyInstance never reaches the physics scene on its own.
      bPhysicsChanged |= ResolvedPath.StartsWith(TEXT("BodyInstance."));
    } else {
      OutWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *PropertyName, *ApplyError));
    }
  }
  if (bPhysicsChanged && Component->IsPhysicsStateCreated()) {
    Component->RecreatePhysicsState();
  }
}

bool McpSendComponentPropertyShortfall(UMcpAutomationBridgeSubsystem &Bridge,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                                       const TArray<FString> &Applied, const TArray<FString> &Warnings,
                                       const TSharedPtr<FJsonObject> &Data, const FString &Prefix) {
  if (Warnings.Num() == 0) {
    return false;
  }
  TArray<TSharedPtr<FJsonValue>> WarnArray;
  for (const FString &Warning : Warnings)
    WarnArray.Add(MakeShared<FJsonValueString>(Warning));
  Data->SetArrayField(TEXT("warnings"), WarnArray);
  // Nothing asked for could be written: that is a failure, not an update.
  if (Applied.Num() == 0) {
    Bridge.SendAutomationResponse(
        Socket, RequestId, false,
        Prefix + FString::Printf(TEXT("No component properties were applied: %s"), *FString::Join(Warnings, TEXT("; "))),
        Data, TEXT("PROPERTY_CONVERSION_FAILED"));
    return true;
  }
  // A partial apply is a failure (PARTIAL_FAILURE) carrying the applied list
  // (dogfood #151); a `warnings` array next to success:true went unread.
  Data->SetBoolField(TEXT("partial"), true);
  Bridge.SendAutomationResponse(
      Socket, RequestId, false,
      Prefix + FString::Printf(TEXT("Applied %d of %d component properties (%d failed): %s"), Applied.Num(),
                      Applied.Num() + Warnings.Num(), Warnings.Num(), *FString::Join(Warnings, TEXT("; "))),
      Data, TEXT("PARTIAL_FAILURE"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
                                TEXT("properties object or propertyName and value required"), nullptr);
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

  // FindComponentByName supports fuzzy matching (numeric suffixes).
  UActorComponent *TargetComponent = FindComponentByName(Found, ComponentName);
  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"), nullptr);
    return true;
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  TargetComponent->Modify();
  McpApplyComponentProperties(TargetComponent, PropertiesObject, AppliedProperties, PropertyWarnings);

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
  McpHandlerUtils::AddVerification(Data, Found);

  if (McpSendComponentPropertyShortfall(*this, Socket, RequestId, AppliedProperties, PropertyWarnings, Data)) {
    return true;
  }
  SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Data);
  return true;
}
