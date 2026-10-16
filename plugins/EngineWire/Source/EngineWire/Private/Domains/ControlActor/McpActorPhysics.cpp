#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"

bool UEngineWireSubsystem::HandleControlActorApplyForce(
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
  // The editor world never steps physics: a push there moved nothing and left the placed actor
  // simulating, a level change nobody asked for.
  const UWorld *World = Found->GetWorld();
  if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("NOT_SIMULATING"),
        TEXT("Physics runs only while the game plays: the editor world never steps it, so a push here "
             "would move nothing. Start play (control_editor play) and push the running game's copy; to "
             "make an actor fall or tumble from the start, set bSimulatePhysics on its mesh component "
             "(control_actor edit_component)."),
        nullptr);
    return true;
  }

  // The body to push: the root when it is one, else a part that simulates, else the first mesh (the
  // first primitive could be a trigger box).
  UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Found->GetRootComponent());
  if (!Prim) {
    TInlineComponentArray<UPrimitiveComponent *> Parts(Found);
    for (UPrimitiveComponent *Part : Parts) {
      if (Part && Part->IsSimulatingPhysics()) {
        Prim = Part;
        break;
      }
    }
  }
  Prim = Prim ? Prim : Found->FindComponentByClass<UMeshComponent>();
  Prim = Prim ? Prim : Found->FindComponentByClass<UPrimitiveComponent>();

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
  }

  // One impulse, not a force: a force lasts one frame, so 60000 moved a crate 0 cm while the reply
  // said it was applied.
  Prim->AddImpulse(ForceVector);
  Prim->WakeAllRigidBodies();

  const bool bIsSimulating = Prim->IsSimulatingPhysics();
  const float Mass = Prim->GetMass();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  Data->SetArrayField(TEXT("applied"), McpHandlerUtils::VectorToJsonArray(ForceVector));
  if (Mass > KINDA_SMALL_NUMBER) {
    Data->SetNumberField(TEXT("massKg"), McpHandlerUtils::ReadableDouble(Mass));
    Data->SetArrayField(TEXT("velocityChange"), McpHandlerUtils::VectorToJsonArray(ForceVector / Mass));
  }
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

	SendAutomationResponse(Socket, RequestId, true,
	    Mass > KINDA_SMALL_NUMBER
	        ? FString::Printf(TEXT("Pushed %s (%.0f kg): %.0f cm/s from the push"), *Prim->GetName(), Mass,
	                          (ForceVector / Mass).Size())
	        : FString(TEXT("Impulse applied")),
	    Data);
  return true;
}
bool UEngineWireSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ActorName;
  bool bCollisionEnabled = true;

  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  const bool bHasEnabled = Payload->HasField(TEXT("collisionEnabled")) || Payload->HasField(TEXT("collision_enabled"));
  if (Payload->HasField(TEXT("collisionEnabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  } else if (Payload->HasField(TEXT("collision_enabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }

  // A preset and per-channel responses (Pawn: Ignore lets a character walk through) are read before anything
  // changes, so a misspelt channel or preset leaves every actor as it was.
  FString Profile, ComponentName, ReadError;
  Payload->TryGetStringField(TEXT("profile"), Profile);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  const TSharedPtr<FJsonObject> *ResponsesObject = nullptr;
  TArray<TPair<ECollisionChannel, ECollisionResponse>> Responses;
  const bool bHasResponses = Payload->TryGetObjectField(TEXT("responses"), ResponsesObject) && ResponsesObject;
  if ((!Profile.IsEmpty() && !McpIsCollisionProfile(Profile, ReadError)) ||
      (bHasResponses && !McpReadCollisionResponses(*ResponsesObject, Responses, ReadError))) {
    SendAutomationError(Socket, RequestId, ReadError + TEXT(". Nothing was changed."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // On or off when asked, or when nothing else is: a preset brings its own mode.
  const bool bSetEnabled = bHasEnabled || (Profile.IsEmpty() && Responses.Num() == 0);

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
  UPrimitiveComponent* First = nullptr;
  for (AActor* Actor : Actors)
  {
    if (bSetEnabled)
    {
      Actor->SetActorEnableCollision(bCollisionEnabled);
    }
    TInlineComponentArray<UPrimitiveComponent*> PrimComponents(Actor);
    int32 OnActor = 0;
    for (UPrimitiveComponent* PrimComp : PrimComponents)
    {
      if (!PrimComp || (!ComponentName.IsEmpty() && !PrimComp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)))
      {
        continue;
      }
      if (!Profile.IsEmpty())
      {
        PrimComp->SetCollisionProfileName(FName(*Profile));
      }
      if (Responses.Num() > 0)
      {
        McpApplyCollisionResponses(PrimComp, Responses);
      }
      if (bSetEnabled)
      {
        PrimComp->SetCollisionEnabled(
            bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics
                              : ECollisionEnabled::NoCollision);
      }
      PrimComp->MarkRenderStateDirty();
      First = First ? First : PrimComp;
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
        !ComponentName.IsEmpty() ? FString::Printf(TEXT("No primitive component named '%s' on %s"), *ComponentName,
                                                   *FString::Join(NoComponent, TEXT(", ")))
        : bMany ? FString::Printf(TEXT("No named actor has a primitive component to configure: %s"),
                                  *FString::Join(NoComponent, TEXT(", ")))
                : FString::Printf(TEXT("Actor '%s' has no primitive components to configure"), *ActorName),
        TEXT("NO_COMPONENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (bSetEnabled)
  {
    Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  }
  // Read back off the first component changed: a preset then responses reads Custom, as the details panel shows.
  if (!Profile.IsEmpty())
  {
    Data->SetStringField(TEXT("profile"), First->GetCollisionProfileName().ToString());
  }
  if (Responses.Num() > 0)
  {
    TSharedPtr<FJsonObject> Now = MakeShared<FJsonObject>();
    for (const TPair<ECollisionChannel, ECollisionResponse>& Entry : Responses)
    {
      Now->SetStringField(UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Entry.Key).ToString(),
                          McpCollisionResponseName(First->GetCollisionResponseToChannel(Entry.Key)));
    }
    Data->SetObjectField(TEXT("responses"), Now);
  }
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
  if (Missing.Num() > 0)
  {
    Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
  }
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
