#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "Containers/Ticker.h"

namespace {
// The switch is queued and runs on the editor's next tick. A session that cannot switch (it plays in its own window
// instead of a level viewport) never does, so the wait is bounded.
constexpr double EjectWaitSeconds = 5.0;
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleControlEditorEject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetBoolField(TEXT("notInPIE"), true);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("Cannot eject: Play session not active"), ErrorDetails);
    return true;
  }

  // There is no "Eject" console command: Exec of one did nothing while the reply said the player was ejected. The
  // editor's own Eject button switches the session from Play to Simulate in Editor. The player leaves its pawn and the
  // level viewport draws the play world from a free camera, which set_camera moves and screenshot photographs.
  const bool bAlreadyEjected = GEditor->bIsSimulatingInEditor;
  if (!bAlreadyEjected) {
    GEditor->RequestToggleBetweenPIEandSIE();
  }

  // The switch runs on the editor's next tick, so the reply waits until the view has switched.
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const double Deadline = FPlatformTime::Seconds() + EjectWaitSeconds;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
      [WeakThis, Socket, RequestId, Deadline, bAlreadyEjected](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }
        const bool bPlaying = GEditor && GEditor->PlayWorld;
        FEditorViewportClient *View = GetEjectedPieViewportClientForMcp();
        if (!View && bPlaying && FPlatformTime::Seconds() < Deadline) {
          return true;
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        if (!View) {
          Resp->SetBoolField(TEXT("ejected"), false);
          WeakThis->SendAutomationResponse(
              Socket, RequestId, false,
              bPlaying ? TEXT("The play session did not switch to a free camera within 5 s. Eject only works while "
                              "Play In Editor runs in a level viewport, not in a window of its own.")
                       : TEXT("Play In Editor ended before the eject finished"),
              Resp, bPlaying ? TEXT("EJECT_FAILED") : TEXT("NO_ACTIVE_SESSION"));
          return false;
        }
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("ejected"), true);
        Resp->SetBoolField(TEXT("alreadyEjected"), bAlreadyEjected);
        Resp->SetStringField(TEXT("view"), TEXT("pie_ejected"));
        Resp->SetObjectField(TEXT("cameraLocation"),
                             McpHandlerUtils::VectorToJson(View->GetViewLocation()));
        Resp->SetObjectField(TEXT("cameraRotation"),
                             McpHandlerUtils::RotatorToJson(View->GetViewRotation()));
        WeakThis->SendAutomationResponse(
            Socket, RequestId, true,
            bAlreadyEjected ? TEXT("The player was already ejected")
                            : TEXT("Ejected: the player left its pawn and the view is a free camera"),
            Resp, FString());
        return false;
      }),
      0.0f);
  return true;
}
