#include "Domains/Animation/Physics/McpAnimPhysicsProp.h"

#include "Domains/Animation/McpAnimActionContext.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace McpAnimationHandlers {
// A crate or barrel handed here by actorName was refused ("does not have a SkeletalMeshComponent"), though this is
// the call search offers for a pushable object. Its body simulates instead: the root when it is a physics body,
// else its first static mesh, which needs collision to have a body at all.
bool SimulatePlacedProp(FActionContext &Context, AActor *Actor) {
  UPrimitiveComponent *Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
  Body = Body ? Body : Actor->FindComponentByClass<UStaticMeshComponent>();
  const UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Body);
  if (!Body || (Mesh && (!Mesh->GetStaticMesh() || !Mesh->GetStaticMesh()->GetBodySetup()))) {
    return false;
  }
  {
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Simulate Physics")),
                                            EMcpMutationDurability::EditorStateOnly, {Body});
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetSimulatePhysics(true);
    Transaction.DescribeInto(Context.Resp);
  }
  Context.Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
  Context.Resp->SetStringField(TEXT("componentName"), Body->GetName());
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
} // namespace McpAnimationHandlers
