#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
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

  // Ensure collision is enabled for physics
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

  // Verify physics state
  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

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

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Data);
  return true;
#else
  return false;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  bool bCollisionEnabled = true;

  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  if (Payload->HasField(TEXT("collisionEnabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  } else if (Payload->HasField(TEXT("collision_enabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }

  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Set collision on root component
  if (USceneComponent* RootComp = Actor->GetRootComponent()) {
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComp)) {
      if (bCollisionEnabled) {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
      } else {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);
  return true;
#else
  return false;
#endif
}
