#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsAsset.h"

bool UMcpAutomationBridgeSubsystem::HandleActivateRagdoll(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("activate_ragdoll"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("activate_ragdoll payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bActivate = true;
  Payload->TryGetBoolField(TEXT("activate"), bActivate);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  AActor *TargetActor = nullptr;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Actor = *It;
    if (Actor) {
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"),
                         FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetBoolField(TEXT("activate"), bActivate);

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

  // Activate or deactivate ragdoll
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
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll activation state changed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("activate_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
