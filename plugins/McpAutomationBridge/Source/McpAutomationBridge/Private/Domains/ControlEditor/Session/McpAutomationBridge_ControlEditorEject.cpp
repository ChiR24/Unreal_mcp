#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#include "Containers/Ticker.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

// possess is the way back from eject, as the editor's own Possess button is: an ejected player returns to the game, and
// actorName, when given, hands the player controller that pawn. It ran Exec("POSSESS"), which is no engine command, so
// nothing was possessed while the reply said it was.
bool UMcpAutomationBridgeSubsystem::HandleControlEditorPossess(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor || !GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IN_PIE"),
                              TEXT("Cannot possess: Play In Editor is not running (control_editor play)"), nullptr);
    return true;
  }
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);
  }
  const bool bWasEjected = GEditor->bIsSimulatingInEditor;
  APawn *Target = nullptr;
  if (!ActorName.IsEmpty()) {
    AActor *Found = FindActorByName(ActorName); // the running game's actor while PIE runs
    if (!Found) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                                FString::Printf(TEXT("Actor not found in the running game: %s"), *ActorName), nullptr);
      return true;
    }
    Target = Cast<APawn>(Found);
    if (!Target) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_TARGET"),
                                FString::Printf(TEXT("Actor '%s' is a %s, not a Pawn; only pawns can be possessed"),
                                                *ActorName, *Found->GetClass()->GetName()), nullptr);
      return true;
    }
  } else if (!bWasEjected) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required: name the pawn to possess. Without it, possess brings an ejected "
                                   "player back to its pawn, and the player is not ejected."), nullptr);
    return true;
  }
  if (bWasEjected) {
    GEditor->RequestToggleBetweenPIEandSIE(); // runs on the next editor tick, like eject
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  TWeakObjectPtr<APawn> WeakTarget(Target);
  const bool bHasTarget = Target != nullptr;
  const double Deadline = FPlatformTime::Seconds() + EjectWaitSeconds;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
      [WeakThis, Socket, RequestId, Deadline, bWasEjected, WeakTarget, bHasTarget](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }
        UWorld *World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (World && GEditor->bIsSimulatingInEditor && FPlatformTime::Seconds() < Deadline) {
          return true;
        }
        APlayerController *Controller = World ? World->GetFirstPlayerController() : nullptr;
        APawn *Pawn = WeakTarget.Get();
        if (!World || GEditor->bIsSimulatingInEditor || !Controller || (bHasTarget && !Pawn)) {
          WeakThis->SendAutomationResponse(
              Socket, RequestId, false,
              !World ? TEXT("Play In Editor ended before the player was back in the game")
              : GEditor->bIsSimulatingInEditor ? TEXT("The ejected player did not return to the game within 5 s")
              : !Controller ? TEXT("The running game has no player controller to possess with")
                            : TEXT("The pawn to possess was destroyed"),
              nullptr, !World ? TEXT("NO_ACTIVE_SESSION") : TEXT("POSSESS_FAILED"));
          return false;
        }
        if (Pawn && Controller->GetPawn() != Pawn) {
          Controller->Possess(Pawn); // a pawn held by another controller is released by Possess itself
        }
        APawn *Held = Controller->GetPawn();
        const bool bPossessed = !bHasTarget || Held == Pawn;
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("possessed"), Held ? Held->GetName() : FString());
        Resp->SetBoolField(TEXT("returnedFromEject"), bWasEjected);
        Resp->SetStringField(TEXT("view"), TEXT("pie_game"));
        WeakThis->SendAutomationResponse(
            Socket, RequestId, bPossessed,
            !bPossessed ? TEXT("The player controller did not take the pawn")
            : bHasTarget ? FString::Printf(TEXT("The player now possesses %s"), *Held->GetName())
                         : FString(TEXT("The ejected player is back in its pawn")),
            Resp, bPossessed ? FString() : TEXT("POSSESS_FAILED"));
        return false;
      }),
      0.0f);
  return true;
}
