#include "McpAutomationBridge_ControlEditorSupport.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewTarget(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);
  }
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), ActorName);
  }

  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  if (!GEditor || !GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
    Details->SetBoolField(TEXT("notInPIE"), true);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IN_PIE"),
                              TEXT("Cannot set game view target while PIE is not active"), Details);
    return true;
  }
  UWorld *PlayWorld = GEditor->PlayWorld.Get();

  double BlendTime = 0.0;
  Payload->TryGetNumberField(TEXT("blendTime"), BlendTime);
  if (BlendTime < 0.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("blendTime must be non-negative"), nullptr);
    return true;
  }

  AActor *TargetActor = FindActorByName(ActorName);
  if (!TargetActor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
    return true;
  }

  AActor *EditorSourceActor = nullptr;
  if (UWorld *EditorWorld = GEditor->GetEditorWorldContext().World()) {
    EditorSourceActor = FindActorByNameInWorldForMcp(EditorWorld, ActorName, true);
  }

  if (TargetActor->GetWorld() != PlayWorld) {
    if (!EditorSourceActor) {
      EditorSourceActor = TargetActor;
    }

    if (EditorSourceActor) {
      AActor *PieActor = FindActorByNameInWorldForMcp(
          PlayWorld, EditorSourceActor->GetActorLabel(), true);
      if (!PieActor) {
        PieActor = FindActorByNameInWorldForMcp(
            PlayWorld, EditorSourceActor->GetName(), true);
      }
      if (PieActor) {
        TargetActor = PieActor;
      }
    }
  }

  if (TargetActor->GetWorld() != PlayWorld) {
    TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
    Details->SetStringField(TEXT("requestedActor"), ActorName);
    Details->SetStringField(TEXT("foundActorPath"), TargetActor->GetPathName());
    if (EditorSourceActor) {
      Details->SetStringField(TEXT("editorActorPath"), EditorSourceActor->GetPathName());
    }
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
        FString::Printf(TEXT("PIE actor not found for view target: %s"), *ActorName),
        Details);
    return true;
  }

  const bool bTargetInPIE = TargetActor->GetWorld() == PlayWorld;
  const bool bCanSyncFromEditor =
      bTargetInPIE && EditorSourceActor && EditorSourceActor != TargetActor;
  const FTransform SourceTransform = bCanSyncFromEditor
      ? EditorSourceActor->GetActorTransform()
      : TargetActor->GetActorTransform();

  const bool bHasLocation = Payload->HasField(TEXT("location"));
  const bool bHasRotation = Payload->HasField(TEXT("rotation"));
  const FVector Location =
      ExtractVectorField(Payload, TEXT("location"), SourceTransform.GetLocation());
  const FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), SourceTransform.Rotator());
  if (bHasLocation || bHasRotation || bCanSyncFromEditor) {
    TargetActor->Modify();
    TargetActor->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
    if (bCanSyncFromEditor) {
      TargetActor->SetActorScale3D(SourceTransform.GetScale3D());
    }
    TargetActor->MarkComponentsRenderStateDirty();
  }

  APlayerController *PlayerController = PlayWorld->GetFirstPlayerController();
  if (!PlayerController) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PLAYER_CONTROLLER_NOT_FOUND"),
                              TEXT("No PlayerController in PIE world"), nullptr);
    return true;
  }

  FViewTargetTransitionParams TransitionParams;
  TransitionParams.BlendTime = static_cast<float>(BlendTime);
  TransitionParams.BlendFunction = VTBlend_Linear;
  TransitionParams.BlendExp = 0.0f;
  TransitionParams.bLockOutgoing = false;
  PlayerController->SetViewTarget(TargetActor, TransitionParams);
  if (bHasRotation || bCanSyncFromEditor) {
    PlayerController->SetControlRotation(Rotation);
  }
  if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager) {
    CameraManager->SetViewTarget(TargetActor, TransitionParams);
    if (bHasLocation || bHasRotation || bCanSyncFromEditor) {
      CameraManager->SetActorLocationAndRotation(
          Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    CameraManager->UpdateCamera(0.0f);

    FMinimalViewInfo TargetView;
    TargetActor->CalcCamera(0.0f, TargetView);
    TargetView.Location = Location;
    TargetView.Rotation = Rotation;
    CameraManager->FillCameraCache(TargetView);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
  Resp->SetStringField(TEXT("actorPath"), TargetActor->GetPathName());
  Resp->SetStringField(TEXT("playerController"), PlayerController->GetPathName());
  Resp->SetNumberField(TEXT("blendTime"), BlendTime);
  Resp->SetBoolField(TEXT("syncedFromEditorWorld"), bCanSyncFromEditor);
  Resp->SetObjectField(TEXT("targetLocation"),
                       MakeVectorObjectForMcp(TargetActor->GetActorLocation()));
  Resp->SetObjectField(TEXT("targetRotation"),
                       MakeRotatorObjectForMcp(TargetActor->GetActorRotation()));
  if (PlayerController->GetViewTarget()) {
    Resp->SetStringField(TEXT("viewTarget"), PlayerController->GetViewTarget()->GetPathName());
  }
  if (PlayerController->PlayerCameraManager) {
    Resp->SetObjectField(TEXT("cameraLocation"),
                         MakeVectorObjectForMcp(PlayerController->PlayerCameraManager->GetCameraLocation()));
    Resp->SetObjectField(TEXT("cameraRotation"),
                         MakeRotatorObjectForMcp(PlayerController->PlayerCameraManager->GetCameraRotation()));
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Game view target set"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : Actors) {
      if (!Actor)
        continue;
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Actor, true, true, true);
        GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
        GEditor->MoveViewportCamerasToActor(*Actor, false);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Viewport focused on actor"), nullptr,
                               FString());
        return true;
      }
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  const TSharedPtr<FJsonObject> *Loc = nullptr;
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadVectorField(*Loc, TEXT(""), Location, Location);
  if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UES =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
      LES->EditorInvalidateViewports();
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
#endif
  if (FEditorViewportClient *ViewportClient =
          GEditor->GetActiveViewport()
              ? (FEditorViewportClient *)GEditor->GetActiveViewport()
                    ->GetClient()
              : nullptr) {
    ViewportClient->SetViewLocation(Location);
    ViewportClient->SetViewRotation(Rotation);
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
  return false;
#else
  return false;
#endif
}
