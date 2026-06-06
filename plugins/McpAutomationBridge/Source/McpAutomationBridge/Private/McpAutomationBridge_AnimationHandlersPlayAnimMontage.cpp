#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandlePlayAnimMontage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("play_anim_montage"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("play_anim_montage payload missing"),
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

  FString MontagePath;
  // Check both montagePath and assetPath for flexibility
  if (!Payload->TryGetStringField(TEXT("montagePath"), MontagePath) ||
      MontagePath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), MontagePath);
  }

  if (MontagePath.IsEmpty()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("error"), TEXT("montagePath required"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("montagePath required"), Resp,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double PlayRate = 1.0;
  Payload->TryGetNumberField(TEXT("playRate"), PlayRate);

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

  // Fallback to ActorSS search if iterator didn't find it (rare but redundant
  // safety)
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
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

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

  if (!UEditorAssetLibrary::DoesAssetExist(MontagePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Montage asset not found: %s"), *MontagePath));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Montage not found"), Resp,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
  if (!Montage) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Failed to load montage: %s"), *MontagePath));
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("montagePath"), MontagePath);
    Resp->SetNumberField(TEXT("playRate"), PlayRate);

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Failed to load montage"), Resp,
                           TEXT("ASSET_LOAD_FAILED"));
    return true;
  }

  float MontageLength = 0.f;
  if (UAnimInstance *AnimInst = SkelMeshComp->GetAnimInstance()) {
    MontageLength =
        AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
  } else {
    SkelMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
    SkelMeshComp->PlayAnimation(Montage, false);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("montagePath"), MontagePath);
  Resp->SetNumberField(TEXT("playRate"), PlayRate);
  Resp->SetNumberField(TEXT("montageLength"), MontageLength);
  Resp->SetBoolField(TEXT("playing"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation montage playing"), Resp, FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("play_anim_montage requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
