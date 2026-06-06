#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddComponent(
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

  FString ComponentType;
  Payload->TryGetStringField(TEXT("componentType"), ComponentType);
  if (ComponentType.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentType required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Component class not found"), nullptr);
    return true;
  }

  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(),
                                    FMath::Rand());

  FName DesiredName = FName(*ComponentName);
  UActorComponent *NewComponent = NewObject<UActorComponent>(
      Found, ComponentClass, DesiredName, RF_Transactional);
  if (!NewComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CREATE_COMPONENT_FAILED"),
                              TEXT("Failed to create component"), nullptr);
    return true;
  }

  Found->Modify();
  NewComponent->SetFlags(RF_Transactional);
  Found->AddInstanceComponent(NewComponent);
  NewComponent->OnComponentCreated();

  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent)) {
    if (Found->GetRootComponent() && !SceneComp->GetAttachParent()) {
      SceneComp->SetupAttachment(Found->GetRootComponent());
    }
  }

  // Force lights to be movable to ensure they work without baking (Issue #6
  // fix) We check for "LightComponent" class name to avoid dependency issues if
  // header is obscure, but ULightComponent is standard.
  if (NewComponent->IsA(ULightComponent::StaticClass())) {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent)) {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  // Special handling for StaticMeshComponent meshPath convenience
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent)) {
    FString MeshPath;
    if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
        !MeshPath.IsEmpty()) {
      const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
      if (!SafeMeshPath.IsEmpty()) {
        if (UObject *LoadedMesh = UEditorAssetLibrary::LoadAsset(SafeMeshPath)) {
          if (UStaticMesh *Mesh = Cast<UStaticMesh>(LoadedMesh)) {
            SMC->SetStaticMesh(Mesh);
          }
        }
      }
    }
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && (*PropertiesPtr).IsValid()) {
    for (const auto &Pair : (*PropertiesPtr)->Values) {
      const FString PropertyName(*Pair.Key);
      FProperty *Property = ComponentClass->FindPropertyByName(*PropertyName);
      if (!Property) {
        PropertyWarnings.Add(
            FString::Printf(TEXT("Property not found: %s"), *PropertyName));
        continue;
      }
      FString ApplyError;
      if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value,
                                   ApplyError))
        AppliedProperties.Add(PropertyName);
      else
        PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                             *PropertyName, *ApplyError));
    }
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
  }
  if (PropertyWarnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warning : PropertyWarnings)
      WarnArray.Add(MakeShared<FJsonValueString>(Warning));
    Resp->SetArrayField(TEXT("warnings"), WarnArray);
	}
	SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);

  // Also accept "objectPath" as an alias, common in inspections
  if (TargetName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), TargetName);
  }

  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or objectPath required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  // Fallback: Check if it's a Blueprint asset to inspect CDO components
  if (!Found) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(TargetName);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeTargetPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
          if (BP->GeneratedClass) {
            Found = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
          }
        }
      }
    }
  }

  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor or Blueprint not found"), nullptr);
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Comp->GetName());
    Entry->SetStringField(TEXT("class"), Comp->GetClass()
                                             ? Comp->GetClass()->GetPathName()
                                             : TEXT(""));
    Entry->SetStringField(TEXT("path"), Comp->GetPathName());
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp)) {
      FVector Loc = SceneComp->GetRelativeLocation();
      FRotator Rot = SceneComp->GetRelativeRotation();
      FVector Scale = SceneComp->GetRelativeScale3D();

      TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
      LocObj->SetNumberField(TEXT("x"), Loc.X);
      LocObj->SetNumberField(TEXT("y"), Loc.Y);
      LocObj->SetNumberField(TEXT("z"), Loc.Z);
      Entry->SetObjectField(TEXT("relativeLocation"), LocObj);

      TSharedPtr<FJsonObject> RotObj = McpHandlerUtils::CreateResultObject();
      RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
      Entry->SetObjectField(TEXT("relativeRotation"), RotObj);

      TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
      ScaleObj->SetNumberField(TEXT("x"), Scale.X);
      ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
      ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
      Entry->SetObjectField(TEXT("relativeScale"), ScaleObj);
    }
    ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

  // Add verification data
  if (Found) {
    McpHandlerUtils::AddVerification(Data, Found);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("component_name"), ComponentName);
  }

  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  if (ComponentName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("componentName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent* Component = FindComponentByName(Actor, ComponentName);
  if (Component) {
    Component->DestroyComponent();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("componentName"), ComponentName);

    // Add verification data for delete operations
    Data->SetBoolField(TEXT("existsAfter"), false);
    Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component removed"), Data);
    return true;
  }

  SendAutomationError(Socket, RequestId,
                      FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                      TEXT("COMPONENT_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}
