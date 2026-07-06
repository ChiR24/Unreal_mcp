#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMediaPlaybackInternal.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpSequenceMedia {

bool HandlePauseMedia(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload,
                      TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Error;
  FString ResolvedPlayerPath;
  UObject *Player = LoadMediaPlayer(Payload, ResolvedPlayerPath, Error);
  if (!Player) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_PLAYER_NOT_FOUND"),
                   Error.IsEmpty() ? TEXT("mediaPlayerPath is required")
                                   : Error);
    return true;
  }
  const bool bCancelledPendingOpen =
      InvalidatePendingMediaPlayback(Player, true);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("mediaPlayerPath"), ResolvedPlayerPath);
  if (bCancelledPendingOpen) {
    Result->SetStringField(TEXT("openStatus"), TEXT("cancelled"));
    Result->SetBoolField(TEXT("pendingOpenCancelled"), true);
    Result->SetBoolField(TEXT("isReady"), false);
    Result->SetBoolField(TEXT("isPlaying"), false);
    Result->SetBoolField(TEXT("isPaused"), false);
    Result->SetStringField(TEXT("currentUrl"), FString());
    Subsystem->SendAutomationResponse(
        Socket, RequestId, true, TEXT("Pending media open cancelled"), Result);
    return true;
  }
  Result->SetBoolField(TEXT("pendingOpenCancelled"), false);
  if (!CallBoolFunction(Player, TEXT("Pause"))) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("MEDIA_PAUSE_FAILED"),
                   TEXT("The media player could not pause playback"), Result);
    return true;
  }
  Result->SetBoolField(TEXT("isPlaying"),
                       CallBoolFunction(Player, TEXT("IsPlaying")));
  Result->SetBoolField(TEXT("isPaused"),
                       CallBoolFunction(Player, TEXT("IsPaused")));
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Media playback paused"), Result);
  return true;
}

bool HandleSeekMedia(UMcpAutomationBridgeSubsystem *Subsystem,
                     const FString &RequestId,
                     const TSharedPtr<FJsonObject> &Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Error;
  FString ResolvedPlayerPath;
  UObject *Player = LoadMediaPlayer(Payload, ResolvedPlayerPath, Error);
  if (!Player) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_PLAYER_NOT_FOUND"),
                   Error.IsEmpty() ? TEXT("mediaPlayerPath is required")
                                   : Error);
    return true;
  }
  const double Seconds =
      GetNumberAny(Payload, {TEXT("timeSeconds"), TEXT("seconds"),
                             TEXT("time")},
                   -1.0);
  if (Seconds < 0.0) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                   TEXT("seek_media requires timeSeconds"));
    return true;
  }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("mediaPlayerPath"), ResolvedPlayerPath);
  Result->SetNumberField(TEXT("timeSeconds"), Seconds);
  if (!CallBoolTimespanFunction(Player, TEXT("Seek"),
                                FTimespan::FromSeconds(Seconds))) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("MEDIA_SEEK_FAILED"),
                   TEXT("The media player could not seek to the requested time"),
                   Result);
    return true;
  }
  Result->SetStringField(TEXT("currentUrl"), GetMediaUrl(Player));
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Media playback seek requested"),
                                    Result);
  return true;
}

}
