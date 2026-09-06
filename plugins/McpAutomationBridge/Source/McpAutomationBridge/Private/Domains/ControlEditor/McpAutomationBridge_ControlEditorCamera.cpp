#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

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

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Move the SAME viewport client the screenshot handler captures. Routing the
  // camera through UUnrealEditorSubsystem::SetLevelViewportCameraInfo used to
  // target the first PERSPECTIVE client in GEditor->GetLevelViewportClients(),
  // which is not necessarily the client that gets drawn and read back, so
  // "set the camera, then take a picture" could address two different viewports.
  FEditorViewportClient *ViewportClient = GetActiveEditorViewportClientForMcp();
  if (!ViewportClient) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
        TEXT("No active level editor viewport to move the camera in"), nullptr);
    return true;
  }

  // The vector lives in a NAMED field of the payload, so it has to be read from
  // the payload. Reading it out of the already-extracted sub-object under an
  // EMPTY field name matched nothing and fell back to the default on every call,
  // which parked the camera at the world origin whatever the caller asked for --
  // and the handler still answered {"success":true}.
  const bool bHasLocation = Payload->HasField(TEXT("location"));
  const bool bHasRotation = Payload->HasField(TEXT("rotation"));
  if (!bHasLocation && !bHasRotation) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        TEXT("location and/or rotation required (object {x,y,z} / {pitch,yaw,roll}, or a 3-number array)"),
        nullptr);
    return true;
  }

  const FVector RequestedLocation = ExtractVectorField(
      Payload, TEXT("location"), ViewportClient->GetViewLocation());
  const FRotator RequestedRotation = ExtractRotatorField(
      Payload, TEXT("rotation"), ViewportClient->GetViewRotation());

  ViewportClient->SetViewLocation(RequestedLocation);
  ViewportClient->SetViewRotation(RequestedRotation);
  ViewportClient->Invalidate();

  // Report where the camera ENDED UP rather than echoing the request. A viewport
  // that refuses to move -- locked to an actor, piloting, orthographic -- is then
  // visible to the caller instead of being reported as a success.
  const FVector AppliedLocation = ViewportClient->GetViewLocation();
  const FRotator AppliedRotation = ViewportClient->GetViewRotation();
  const bool bLocationApplied = AppliedLocation.Equals(RequestedLocation, 1.0);
  const bool bRotationApplied = AppliedRotation.Equals(RequestedRotation, 1.0);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bLocationApplied && bRotationApplied);
  Resp->SetObjectField(TEXT("requestedLocation"),
                       MakeVectorObjectForMcp(RequestedLocation));
  Resp->SetObjectField(TEXT("requestedRotation"),
                       MakeRotatorObjectForMcp(RequestedRotation));
  Resp->SetObjectField(TEXT("cameraLocation"),
                       MakeVectorObjectForMcp(AppliedLocation));
  Resp->SetObjectField(TEXT("cameraRotation"),
                       MakeRotatorObjectForMcp(AppliedRotation));
  Resp->SetBoolField(TEXT("locationApplied"), bLocationApplied);
  Resp->SetBoolField(TEXT("rotationApplied"), bRotationApplied);
  Resp->SetBoolField(TEXT("perspective"), ViewportClient->IsPerspective());

  if (!bLocationApplied || !bRotationApplied) {
    const FString Message =
        TEXT("Viewport did not take the requested camera. It may be locked to an "
             "actor, piloting, or orthographic; cameraLocation/cameraRotation "
             "report where it actually is.");
    Resp->SetStringField(TEXT("error"), Message);
    Resp->SetStringField(TEXT("message"), Message);
    SendAutomationResponse(Socket, RequestId, false, Message, Resp,
                           TEXT("CAMERA_NOT_APPLIED"));
    return true;
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}