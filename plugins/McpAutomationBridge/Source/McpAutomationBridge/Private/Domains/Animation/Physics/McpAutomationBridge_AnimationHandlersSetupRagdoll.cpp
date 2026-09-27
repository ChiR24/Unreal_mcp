#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Subsystems/EditorActorSubsystem.h"

// setup_ragdoll (optionally assigning physicsAssetPath) and activate_ragdoll (activate, default true) both switch the
// actor's skeletal mesh into or out of physics simulation; the parent animation_physics route calls this for both.
bool UMcpAutomationBridgeSubsystem::HandleSetupRagdoll(
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
  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // During PIE the play world holds the live actors; the editor world otherwise.
  UWorld *PieWorld = GEditor->PlayWorld;
  AActor *TargetActor = FindActorByNameInWorldForMcp(
      PieWorld ? PieWorld : GEditor->GetEditorWorldContext().World(), ActorName, true);
  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"), FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Actor not found"), Resp,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USkeletalMeshComponent *SkelMeshComp =
      TargetActor->FindComponentByClass<USkeletalMeshComponent>();
  if (!SkelMeshComp) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Skeletal mesh component not found"),
                        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Optional explicit physics asset (dogfood #143); the mesh default is used otherwise.
  const FString PhysicsAssetPath = bSetup ? GetJsonStringField(Payload, TEXT("physicsAssetPath")) : FString();
  if (!PhysicsAssetPath.IsEmpty()) {
    UPhysicsAsset *RagdollAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetPath);
    if (!RagdollAsset) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Physics asset not found: %s"), *PhysicsAssetPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    SkelMeshComp->SetPhysicsAsset(RagdollAsset, true);
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
