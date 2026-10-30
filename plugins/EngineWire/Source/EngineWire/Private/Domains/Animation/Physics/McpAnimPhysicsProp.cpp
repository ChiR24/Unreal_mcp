#include "Domains/Animation/Physics/McpAnimPhysicsProp.h"

#include "Domains/Animation/McpAnimActionContext.h"
#include "Domains/Animation/McpAnimActionDecls.h"
#include "EngineWireSubsystem.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace McpAnimationHandlers {
// A crate or barrel handed here by actorName was refused ("does not have a SkeletalMeshComponent"), though this is
// the call search offers for a pushable object. Its body simulates instead: the root when it is a physics body,
// else its first static mesh, which needs collision to have a body at all.
bool SimulatePlacedProp(FActionContext &Context, AActor *Actor, const TSharedPtr<FJsonObject> &Payload) {
  UPrimitiveComponent *Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
  Body = Body ? Body : Actor->FindComponentByClass<UStaticMeshComponent>();
  const UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Body);
  if (!Body || (Mesh && (!Mesh->GetStaticMesh() || !Mesh->GetStaticMesh()->GetBodySetup()))) {
    return false;
  }
  double MassKg = 0.0; // in range: HandleSetupPhysicsSimulation refused any other
  Payload->TryGetNumberField(TEXT("massKg"), MassKg);
  {
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Simulate Physics")),
                                            EMcpMutationDurability::EditorStateOnly, {Body});
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetSimulatePhysics(true);
    if (MassKg > 0.0) {
      Body->SetMassOverrideInKg(NAME_None, static_cast<float>(MassKg), true);
    }
    Transaction.DescribeInto(Context.Resp);
  }
  Context.Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
  Context.Resp->SetStringField(TEXT("componentName"), Body->GetName());
  // The override when one is set, else what its collision shapes and physical material give it.
  Context.Resp->SetNumberField(TEXT("massKg"), FMath::RoundToDouble(Body->CalculateMass() * 100.0) / 100.0);
  // A simulating part that is not the root leaves the actor behind: the actor's location, and every read of it,
  // stays where it was placed while the mesh falls away.
  if (Body != Actor->GetRootComponent()) {
    Context.Resp->SetArrayField(TEXT("warnings"), {MakeShared<FJsonValueString>(FString::Printf(
        TEXT("'%s' is not the actor's root, so it falls and moves on its own while the actor's location stays where "
             "it was placed. Make it the root (manage_blueprint edit_scs) for an actor that moves as one."),
        *Body->GetName()))});
  }
  Context.bSuccess = true;
  Context.Message = FString::Printf(
      TEXT("%s simulates physics on '%s': it falls, tumbles and can be pushed while the game plays."),
      *Actor->GetActorLabel(), *Body->GetName());
  return true;
}

bool HandleSetupPhysicsSimulation(FActionContext &Context, const TSharedPtr<FJsonObject> &Payload) {
  double MassKg = 0.0;
  if (Payload->TryGetNumberField(TEXT("massKg"), MassKg)) {
    FString SkeletonPath, MeshPath, ActorName;
    Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath);
    Payload->TryGetStringField(TEXT("skeletalMeshPath"), MeshPath);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    const AActor *Actor = ActorName.IsEmpty() ? nullptr : Context.FindActorByName(ActorName);
    const bool bSkeletal = !SkeletonPath.IsEmpty() || !MeshPath.IsEmpty() ||
                           (Actor && Actor->FindComponentByClass<USkeletalMeshComponent>());
    const TCHAR *Refusal = !(MassKg >= 0.01 && MassKg <= 100000.0) ? TEXT("massKg is from 0.01 to 100000 kg")
                           : bSkeletal ? TEXT("massKg weighs a placed prop; a skeletal mesh's bodies are weighed in "
                                              "its Physics Asset (configure_physics_body)")
                                       : nullptr;
    if (Refusal) {
      Context.Fail(TEXT("INVALID_ARGUMENT"), Refusal);
      Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, Context.Message,
                                            Context.Resp, Context.ErrorCode);
      return true;
    }
  }
  return HandleAnimationSetupPhysicsSimulationAction(Context, Payload);
}
} // namespace McpAnimationHandlers
