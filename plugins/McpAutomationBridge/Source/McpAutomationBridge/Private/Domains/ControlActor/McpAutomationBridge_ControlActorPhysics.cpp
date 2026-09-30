#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FVector ForceVector =
      ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UPrimitiveComponent *Prim =
      Found->FindComponentByClass<UPrimitiveComponent>();
  if (!Prim) {
    if (UStaticMeshComponent *SMC =
            Found->FindComponentByClass<UStaticMeshComponent>())
      Prim = SMC;
  }

  if (!Prim) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_COMPONENT"),
                              TEXT("No component to apply force"), nullptr);
    return true;
  }

  if (Prim->Mobility == EComponentMobility::Static)
    Prim->SetMobility(EComponentMobility::Movable);

  if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
    Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }

  // Check if collision geometry exists (common failure for empty
  // StaticMeshActors)
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Prim)) {
    if (!SMC->GetStaticMesh()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMeshComponent has no StaticMesh assigned."), nullptr);
      return true;
    }
    if (!SMC->GetStaticMesh()->GetBodySetup()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMesh has no collision geometry (BodySetup is null)."),
          nullptr);
      return true;
    }
  }

  if (!Prim->IsSimulatingPhysics()) {
    Prim->SetSimulatePhysics(true);
    // Must recreate physics state for the body to be properly initialized in
    // Editor
    Prim->RecreatePhysicsState();
  }

  Prim->AddForce(ForceVector);
  Prim->WakeAllRigidBodies();
  Prim->MarkRenderStateDirty();

  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), McpActorRef(Found));

  if (!bIsSimulating) {
    FString FailureReason = TEXT("Failed to enable physics simulation.");
    if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
      FailureReason += TEXT(" Collision is disabled.");
    } else if (Prim->Mobility != EComponentMobility::Movable) {
      FailureReason += TEXT(" Component is not Movable.");
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
                              FailureReason, Data);
    return true;
  }

	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Data);
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ActorName;
  bool bCollisionEnabled = true;

  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  if (Payload->HasField(TEXT("collisionEnabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  } else if (Payload->HasField(TEXT("collision_enabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }

  // actorNames: many actors in one call (a level's trigger volumes took one call each). Names that
  // matched nothing are listed back, as add_tag lists them; the single-actor replies are unchanged.
  TArray<AActor*> Actors;
  TArray<FString> Missing;
  const bool bMany = McpResolveActorNames(
      Payload, [this](const FString& Name) { return FindActorByName(Name); }, Actors, Missing);
  if (bMany && Actors.Num() == 0) {
    McpSendNoActorNamesFound(this, Socket, RequestId, Missing);
    return true;
  }
  if (!bMany) {
    if (ActorName.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("actorName (or actorNames) is required"), TEXT("MISSING_PARAM"));
      return true;
    }
    AActor* Found = FindActorByName(ActorName);
    if (!Found) {
      SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
      return true;
    }
    Actors.Add(Found);
  }

  // One transaction for the whole call, as set_visibility opens one: every actor and primitive
  // component changed below, each flagged RF_Transactional where the engine made it without, so a
  // single undo takes the whole list back and the reply says it can.
  TArray<UObject*> Undoable;
  for (AActor* Actor : Actors)
  {
    McpAddActorUndoSet(Actor, Undoable);
  }
  FMcpScopedEditorTransaction Transaction(
      FText::FromString(TEXT("Set Actor Collision")),
      EMcpMutationDurability::EditorStateOnly, Undoable);

  // Two-part fix: (1) flip the authoritative actor-level switch, (2) apply the
  // collision mode to EVERY primitive component, not just the root. The old
  // root-only loop silently skipped attached components (e.g. a DynamicMesh
  // actor's component chain) and reported success while nothing changed.
  int32 Updated = 0;
  TArray<FString> Affected;
  TArray<FString> NoComponent;
  for (AActor* Actor : Actors)
  {
    Actor->SetActorEnableCollision(bCollisionEnabled);
    TInlineComponentArray<UPrimitiveComponent*> PrimComponents(Actor);
    int32 OnActor = 0;
    for (UPrimitiveComponent* PrimComp : PrimComponents)
    {
      if (!PrimComp)
      {
        continue;
      }
      PrimComp->SetCollisionEnabled(
          bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics
                            : ECollisionEnabled::NoCollision);
      PrimComp->MarkRenderStateDirty();
      ++OnActor;
    }
    if (OnActor == 0)
    {
      NoComponent.Add(McpActorRef(Actor));
    }
    else
    {
      Affected.Add(McpActorRef(Actor));
    }
    Updated += OnActor;
  }

  if (Updated == 0)
  {
    SendAutomationError(Socket, RequestId,
        bMany ? FString::Printf(TEXT("No named actor has a primitive component to configure: %s"),
                                *FString::Join(NoComponent, TEXT(", ")))
              : FString::Printf(TEXT("Actor '%s' has no primitive components to configure"), *ActorName),
        TEXT("NO_COMPONENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  Data->SetNumberField(TEXT("componentsUpdated"), Updated);
  Transaction.DescribeInto(Data);
  if (!bMany)
  {
    Data->SetStringField(TEXT("actorName"), ActorName);
    McpHandlerUtils::AddVerification(Data, Actors[0]);
    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);
    return true;
  }
  // The actors with a primitive component this call updated: the receipt lists them as changes, as
  // it lists the single form's actorName.
  Data->SetNumberField(TEXT("updatedActors"), Affected.Num());
  Data->SetArrayField(TEXT("affectedActors"), McpHandlerUtils::ToJsonStringArray(Affected));
  Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
  if (NoComponent.Num() > 0)
  {
    Data->SetArrayField(TEXT("noPrimitiveComponents"), McpHandlerUtils::ToJsonStringArray(NoComponent));
  }
  SendStandardSuccessResponse(this, Socket, RequestId,
      FString::Printf(TEXT("Collision set on %d actor(s); %d not found"), Affected.Num(),
                      Missing.Num()),
      Data);
  return true;
}
