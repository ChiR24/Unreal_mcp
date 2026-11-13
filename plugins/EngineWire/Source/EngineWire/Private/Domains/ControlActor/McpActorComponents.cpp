#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

bool UEngineWireSubsystem::HandleControlActorAddComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("actorName"), TargetName))
    return true;

  FString ComponentType;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("componentType"), ComponentType))
    return true;

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = FindActorNamedOrReply(TargetName, RequestId, Socket);
  if (!Found)
    return true;

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  // An abstract class (PrimitiveComponent, MeshComponent) makes a component the editor warns about and nulls on save.
  const bool bAbstract = ComponentClass && ComponentClass->HasAnyClassFlags(CLASS_Abstract);
  if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()) || bAbstract) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
        bAbstract ? TEXT("Component class is abstract and cannot be added") : TEXT("Component class not found"), nullptr);
    return true;
  }

  // A free name from the engine when none is given: the random suffix used before could land on a taken one.
  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = MakeUniqueObjectName(Found, ComponentClass, ComponentClass->GetFName()).ToString();
  // NewObject over a subobject of that name is a Fatal log when it is of another class, and for one of the same class
  // rebuilt the actor's own component in place (an engine ensure) while the reply called it added.
  if (const UObject *Holder = FindObject<UObject>(Found, *ComponentName)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ALREADY_EXISTS"),
        FString::Printf(TEXT("Component '%s' already exists on %s as a %s; pick another componentName. Nothing was added."),
                        *ComponentName, *TargetName, *Holder->GetClass()->GetName()), nullptr);
    return true;
  }

  // One undo step, as remove_component and the property edits are: Modify alone left nothing to take back.
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Component")), EMcpMutationDurability::EditorStateOnly,
                                          {Found});
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

  // A light is made Movable, so it lights the scene without a bake.
  if (NewComponent->IsA(ULightComponent::StaticClass())) {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent)) {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  // A mesh that failed to load, or a meshPath on a component that takes none,
  // used to be dropped without a word.
  FString MeshPath;
  if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) && !MeshPath.IsEmpty()) {
    UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent);
    const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    UStaticMesh *Mesh = (SMC && !SafeMeshPath.IsEmpty())
                            ? Cast<UStaticMesh>(McpLoadAsset(SafeMeshPath))
                            : nullptr;
    if (Mesh) {
      SMC->SetStaticMesh(Mesh);
      AppliedProperties.Add(TEXT("meshPath"));
    } else {
      PropertyWarnings.Add(SMC ? FString::Printf(TEXT("Failed to set meshPath: no static mesh could be loaded from %s"), *MeshPath)
                               : FString(TEXT("Failed to set meshPath: only a StaticMeshComponent takes a mesh")));
    }
  }
  // The same property bag set_component_properties writes, so nested paths
  // (BodyInstance.CollisionEnabled) and the engine setters work here too.
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && (*PropertiesPtr).IsValid()) {
    McpApplyComponentProperties(NewComponent, *PropertiesPtr, AppliedProperties, PropertyWarnings);
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  if (AppliedProperties.Num() > 0) {
    Resp->SetArrayField(TEXT("appliedProperties"), McpHandlerUtils::ToJsonStringArray(AppliedProperties));
  }
  // Every requested property failing came back success:true with the failures
  // in warnings; it now fails the way set_component_properties does, naming
  // the component that was added.
  if (McpSendComponentPropertyShortfall(*this, Socket, RequestId, AppliedProperties, PropertyWarnings, Resp,
                                        FString::Printf(TEXT("Component %s was added. "), *NewComponent->GetName()))) {
    return true;
  }
  SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp);
  return true;
}
bool UEngineWireSubsystem::HandleControlActorGetComponents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
      if (UObject *Asset = McpLoadAsset(SafeTargetPath)) {
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

      Entry->SetObjectField(TEXT("relativeLocation"), McpHandlerUtils::VectorToJson(Loc));

      Entry->SetObjectField(TEXT("relativeRotation"), McpHandlerUtils::RotatorToJson(Rot));

      Entry->SetObjectField(TEXT("relativeScale"), McpHandlerUtils::VectorToJson(Scale));
      // A part under a scaled parent is drawn at that scale times its own: relativeScale alone hid that a trigger
      // box of extent 70x70x40 was 112x112x4 in the world. A class default's parts have no world transform.
      if (!Found->HasAnyFlags(RF_ClassDefaultObject) && !SceneComp->GetComponentScale().Equals(Scale, 1e-3)) {
        Entry->SetObjectField(TEXT("worldScale"), McpHandlerUtils::VectorToJson(SceneComp->GetComponentScale()));
      }
    }

    // Inspection detail (class identity, attach parent, visibility/active,
    // bounded property census) is appended by the shared component-detail
    // helper so this list handler stays within its line budget.
    McpAppendComponentDetailFields(Comp, Entry);

    ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::FilterRowsByListedNames(Payload, TEXT("componentNames"), ComponentsArray, Data,
                                           TEXT("missingComponents"));
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

  // Found is guaranteed non-null here: the !Found path already returned.
  McpHandlerUtils::AddVerification(Data, Found);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
}
bool UEngineWireSubsystem::HandleControlActorRemoveComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

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

  // Only the component the caller named: the lookup also accepts a prefix or part of the path, which for a removal
  // took "Mesh" to StaticMeshComponent0. A near miss is named back instead.
  UActorComponent* Component = FindComponentByName(Actor, ComponentName);
  const bool bExact = Component && (Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
                                    Component->GetPathName().EndsWith(TEXT(".") + ComponentName, ESearchCase::IgnoreCase) ||
                                    Component->GetPathName().EndsWith(TEXT(":") + ComponentName, ESearchCase::IgnoreCase));
  if (!bExact) {
    SendAutomationError(Socket, RequestId,
                        Component ? FString::Printf(TEXT("No component named %s; did you mean %s?"), *ComponentName, *Component->GetName())
                                  : FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }
  // Only a component added to this actor goes: the class or its Blueprint rebuilds the others (the construction
  // script on the next move), which the reply used to call removed.
  if (Component->CreationMethod != EComponentCreationMethod::Instance || Component == Actor->GetRootComponent()) {
    SendAutomationError(Socket, RequestId,
                        Component == Actor->GetRootComponent()
                            ? FString::Printf(TEXT("%s is the actor's root; removing it would drop everything under it"), *Component->GetName())
                        : Component->CreationMethod == EComponentCreationMethod::SimpleConstructionScript
                            ? FString::Printf(TEXT("%s comes from the actor's Blueprint and would come back: remove it there with edit_scs"), *Component->GetName())
                            : FString::Printf(TEXT("%s is part of the actor's class, not added to this actor, so it cannot be removed"), *Component->GetName()),
                        TEXT("COMPONENT_NOT_REMOVABLE"));
    return true;
  }
  // One undo step, and the level is marked unsaved: the removal was neither, so a save left the component in.
  const FString Removed = Component->GetName();
  {
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Component")),
                                            EMcpMutationDurability::EditorStateOnly, {Actor, Component});
    Component->DestroyComponent();
    Actor->MarkPackageDirty();
  }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), McpActorRef(Actor));
  Data->SetStringField(TEXT("componentName"), Removed);
  Data->SetBoolField(TEXT("existsAfter"), false);
  Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component removed"), Data);
  return true;
}
