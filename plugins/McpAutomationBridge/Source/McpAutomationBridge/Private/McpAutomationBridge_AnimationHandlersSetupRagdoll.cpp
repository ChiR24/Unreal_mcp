#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSetupRagdoll(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("setup_ragdoll"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("setup_ragdoll payload missing"),
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

  double BlendWeight = 1.0;
  Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight);

  FString SkeletonPath;
  if (Payload->TryGetStringField(TEXT("skeletonPath"), SkeletonPath) &&
      !SkeletonPath.IsEmpty()) {
    USkeleton *RagdollSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
    if (!RagdollSkeleton) {
      const FString SkelMessage =
          FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath);
      SendAutomationError(RequestingSocket, RequestId, SkelMessage,
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *TargetActor = nullptr;

  if (GEditor && GEditor->GetEditorWorldContext().World()) {
    UWorld *World = GEditor->GetEditorWorldContext().World();
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
  }

  if (!TargetActor) {
    for (AActor *Actor : AllActors) {
      if (Actor &&
          (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
           Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase))) {
        TargetActor = Actor;
        break;
      }
    }
  }

  if (!TargetActor) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);

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

  SkelMeshComp->SetSimulatePhysics(true);
  SkelMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

  if (SkelMeshComp->GetPhysicsAsset()) {
    SkelMeshComp->SetAllBodiesSimulatePhysics(true);
    SkelMeshComp->SetUpdateAnimationInEditor(BlendWeight < 1.0);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetNumberField(TEXT("blendWeight"), BlendWeight);
  Resp->SetBoolField(TEXT("ragdollActive"),
                     SkelMeshComp->IsSimulatingPhysics());
  Resp->SetBoolField(TEXT("hasPhysicsAsset"),
                     SkelMeshComp->GetPhysicsAsset() != nullptr);

  if (SkelMeshComp->GetPhysicsAsset()) {
    Resp->SetStringField(TEXT("physicsAssetPath"),
                         SkelMeshComp->GetPhysicsAsset()->GetPathName());
  }

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Ragdoll setup completed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("setup_ragdoll requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
