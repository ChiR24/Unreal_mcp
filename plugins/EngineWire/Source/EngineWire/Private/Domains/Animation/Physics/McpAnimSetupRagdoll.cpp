#include "EngineWireSubsystem.h"
#include "Domains/Animation/McpAnimActionDecls.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpAnimationHandlers {
USkeletalMeshComponent *FindSkeletalMeshOrReply(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString &ActorName,
    bool &bOutPlaying, TFunctionRef<void(FJsonObject &)> AddNotFoundFields) {
  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("Editor world not available"),
                               TEXT("EDITOR_NOT_AVAILABLE"));
    return nullptr;
  }

  // During PIE the play world holds the live actors; the editor world otherwise.
  UWorld *PieWorld = GEditor->PlayWorld;
  bOutPlaying = PieWorld != nullptr;
  AActor *TargetActor = FindActorByNameInWorldForMcp(
      PieWorld ? PieWorld : GEditor->GetEditorWorldContext().World(), ActorName, true);
  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"), FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    AddNotFoundFields(*Resp);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Actor not found"), Resp,
                                  TEXT("ACTOR_NOT_FOUND"));
    return nullptr;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("Skeletal mesh component not found"),
                               TEXT("COMPONENT_NOT_FOUND"));
  }
  return SkelMeshComp;
}
} // namespace McpAnimationHandlers

// setup_ragdoll (optionally assigning physicsAssetPath) and activate_ragdoll (activate, default true) both switch the
// actor's skeletal mesh into or out of physics simulation; the parent animation_physics route calls this for both.
bool UEngineWireSubsystem::HandleSetupRagdoll(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const bool bSetup = Action.Equals(TEXT("setup_ragdoll"), ESearchCase::IgnoreCase);
  const bool bActivate = bSetup || GetJsonBoolField(Payload, TEXT("activate"), true);
  const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
  if (ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  bool bPlaying = false;
  USkeletalMeshComponent *SkelMeshComp = McpAnimationHandlers::FindSkeletalMeshOrReply(
      *this, RequestId, RequestingSocket, ActorName, bPlaying);
  if (!SkelMeshComp) {
    return true;
  }

  // Optional explicit physics asset; the mesh default is used otherwise.
  const FString PhysicsAssetPath = bSetup ? GetJsonStringField(Payload, TEXT("physicsAssetPath")) : FString();
  if (!PhysicsAssetPath.IsEmpty()) {
    UPhysicsAsset *RagdollAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetPath);
    if (!RagdollAsset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Physics asset not found: %s"), *PhysicsAssetPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!bPlaying) {
      SkelMeshComp->Modify();
    }
    SkelMeshComp->SetPhysicsAsset(RagdollAsset, true);
  }
  // Without a physics asset the mesh has no bodies to simulate, so it never went limp while the reply said done.
  if (bActivate && !SkelMeshComp->GetPhysicsAsset()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("%s's skeletal mesh has no physics asset, so it has no bodies to go limp; pass physicsAssetPath (or give the mesh one)."), *ActorName),
                        TEXT("NO_PHYSICS_ASSET"));
    return true;
  }
  // In the editor this is the placed actor's own setting: it starts the game simulating (limp from the first frame).
  if (!bPlaying) {
    SkelMeshComp->Modify();
  }
  if (bActivate) {
    SkelMeshComp->SetSimulatePhysics(true);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (SkelMeshComp->GetPhysicsAsset()) {
      SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    }
  } else {
    SkelMeshComp->SetAllBodiesSimulatePhysics(false);
    SkelMeshComp->SetSimulatePhysics(false);
    SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetBoolField(TEXT("activate"), bActivate);
  Resp->SetBoolField(TEXT("ragdollActive"), SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"), SkelMeshComp->GetPhysicsAsset() != nullptr);
  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"), SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         bSetup ? TEXT("Ragdoll setup completed") : TEXT("Ragdoll activation state changed"),
                         Resp, FString());
  return true;
}
