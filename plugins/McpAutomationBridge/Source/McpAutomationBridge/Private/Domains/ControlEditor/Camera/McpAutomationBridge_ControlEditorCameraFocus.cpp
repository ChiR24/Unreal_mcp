#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

#include "Subsystems/EditorActorSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), ActorName);
  }
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);
  }
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  AActor *Target = FindActorByNameInWorldForMcp(
      GEditor->GetEditorWorldContext().World(), ActorName, true);

  if (!Target) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
        FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
    return true;
  }

  GEditor->SelectNone(true, true, false);
  GEditor->SelectActor(Target, true, true, true);
  GEditor->MoveViewportCamerasToActor(*Target, false);

  // MoveViewportCamerasToActor ANIMATES the camera: it ends in FocusViewportOnBox,
  // whose bInstant parameter defaults to false. A screenshot taken straight after
  // this call therefore catches the camera in flight, and an automation client has
  // no way to observe when the flight is over. Land the viewport we actually
  // capture on the target immediately so "focus then screenshot" is deterministic.
  FEditorViewportClient *ViewportClient = GetActiveEditorViewportClientForMcp();
  const FBox FocusBox = Target->GetComponentsBoundingBox(true);
  const bool bBoundsValid = FocusBox.IsValid != 0;
  if (ViewportClient && bBoundsValid) {
    ViewportClient->FocusViewportOnBox(FocusBox, /*bInstant=*/true);
    ViewportClient->Invalidate();
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), McpActorRef(Target));
  Resp->SetStringField(TEXT("actorPath"), Target->GetPathName());
  Resp->SetBoolField(TEXT("boundsValid"), bBoundsValid);
  Resp->SetBoolField(TEXT("focusedInstantly"), ViewportClient && bBoundsValid);
  if (bBoundsValid) {
    Resp->SetObjectField(TEXT("focusCenter"),
                         McpHandlerUtils::VectorToJson(FocusBox.GetCenter()));
    Resp->SetObjectField(TEXT("focusExtent"),
                         McpHandlerUtils::VectorToJson(FocusBox.GetExtent()));
  }
  if (ViewportClient) {
    Resp->SetObjectField(TEXT("cameraLocation"),
                         McpHandlerUtils::VectorToJson(ViewportClient->GetViewLocation()));
    Resp->SetObjectField(TEXT("cameraRotation"),
                         McpHandlerUtils::RotatorToJson(ViewportClient->GetViewRotation()));
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport focused on actor"), Resp, FString());
  return true;
}