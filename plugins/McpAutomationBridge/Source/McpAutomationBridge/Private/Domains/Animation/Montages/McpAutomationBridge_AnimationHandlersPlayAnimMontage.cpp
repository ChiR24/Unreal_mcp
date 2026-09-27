#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandlePlayAnimMontage(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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

  // During PIE the play world holds the live actors; the editor world otherwise.
  UWorld *PieWorld = GEditor->PlayWorld;
  AActor *TargetActor = FindActorByNameInWorldForMcp(
      PieWorld ? PieWorld : GEditor->GetEditorWorldContext().World(), ActorName, true);

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

  // Resolve the montage by loading it (package path or object path); DoesAssetExist rejected valid
  // /Game package paths for freshly authored montages (dogfood #91).
  UAnimMontage *Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
  if (!Montage && !MontagePath.Contains(TEXT("."))) {
    Montage = LoadObject<UAnimMontage>(
        nullptr, *(MontagePath + TEXT(".") + FPackageName::GetShortName(MontagePath)));
  }
  if (!Montage) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Montage asset not found: %s"), *MontagePath));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Montage not found"), Resp,
                           TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  float MontageLength = 0.f;
  UAnimInstance *AnimInst = SkelMeshComp->GetAnimInstance();
  if (AnimInst) {
    MontageLength =
        AnimInst->Montage_Play(Montage, static_cast<float>(PlayRate));
    if (MontageLength <= 0.f) {
      // Montage_Play answers 0 when it refused, e.g. for another skeleton.
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("Montage_Play refused %s on %s; check that it uses the mesh's skeleton"), *MontagePath, *ActorName),
                          TEXT("MONTAGE_PLAY_FAILED"));
      return true;
    }
  } else {
    // Without an AnimInstance the montage plays as a single-node asset;
    // PlayAnimation ignores the rate and the reply used to claim it anyway.
    SkelMeshComp->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
    SkelMeshComp->PlayAnimation(Montage, false);
    SkelMeshComp->SetPlayRate(static_cast<float>(PlayRate));
    MontageLength = Montage->GetPlayLength();
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("playMode"), AnimInst ? TEXT("montage") : TEXT("singleNode"));
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("montagePath"), MontagePath);
  Resp->SetNumberField(TEXT("playRate"), PlayRate);
  Resp->SetNumberField(TEXT("montageLength"), MontageLength);
  Resp->SetBoolField(TEXT("playing"), true);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Animation montage playing"), Resp, FString());
  return true;
}
