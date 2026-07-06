#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMediaPlaybackInternal.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpSequenceMedia {
namespace {
bool ValidateMediaOpenRequestAliases(const TSharedPtr<FJsonObject> &Payload,
                                     bool &OutHasOpenRequest,
                                     FString &OutError) {
  OutHasOpenRequest = false;
  if (!Payload.IsValid())
    return true;
  static const TCHAR *Fields[] = {
      TEXT("mediaSourcePath"), TEXT("sourcePath"), TEXT("playlistPath"),
      TEXT("url"), TEXT("streamUrl"), TEXT("filePath"), TEXT("mediaPath")};
  for (const TCHAR *Field : Fields) {
    if (!Payload->HasField(Field))
      continue;
    OutHasOpenRequest = true;
    FString Value;
    if (!Payload->TryGetStringField(Field, Value) ||
        Value.TrimStartAndEnd().IsEmpty()) {
      OutError = TEXT("A supplied media open value must not be empty.");
      return false;
    }
  }
  return true;
}
}

bool TryHandleMediaAction(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          const FString &Action,
                          const TSharedPtr<FJsonObject> &Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString MediaAction = CanonicalMediaAction(Action);
  if (MediaAction == TEXT("create_media_player")) {
    return HandleCreateMediaPlayer(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("create_media_source")) {
    return HandleCreateMediaSource(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("create_media_texture")) {
    return HandleCreateMediaTexture(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("create_media_sound_component")) {
    return HandleCreateMediaSoundComponent(Subsystem, RequestId, Payload,
                                           Socket);
  }
  if (MediaAction == TEXT("create_media_playlist")) {
    return HandleCreateMediaPlaylist(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("play_media")) {
    return HandlePlayMedia(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("pause_media")) {
    return HandlePauseMedia(Subsystem, RequestId, Payload, Socket);
  }
  if (MediaAction == TEXT("seek_media")) {
    return HandleSeekMedia(Subsystem, RequestId, Payload, Socket);
  }
  return false;
}

bool HandlePlayMedia(UMcpAutomationBridgeSubsystem *Subsystem,
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
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("mediaPlayerPath"), ResolvedPlayerPath);
  FString OpenErrorCode;
  FString OpenErrorMessage;
  FString ExpectedUrl;
  bool bOpenRequested = false;
  bool bHasOpenRequest = false;
  if (!ValidateMediaOpenRequestAliases(Payload, bHasOpenRequest,
                                       OpenErrorMessage)) {
    SendMediaError(Subsystem, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                   OpenErrorMessage, Result);
    return true;
  }
  if (bHasOpenRequest) {
    InvalidatePendingMediaPlayback(Player, true);
    CallVoidFunction(Player, TEXT("Close"));
  }
  if (!OpenRequestedMedia(Player, Payload, Result, bOpenRequested,
                          OpenErrorCode, OpenErrorMessage, ExpectedUrl)) {
    SendMediaError(Subsystem, Socket, RequestId, OpenErrorCode,
                   OpenErrorMessage, Result);
    return true;
  }
  if (!bOpenRequested) {
    ExpectedUrl = GetMediaUrl(Player);
  }
  StartMediaPlaybackAfterOpen(Subsystem, RequestId, Socket, Player, Result,
                              ExpectedUrl);
  return true;
}

}
