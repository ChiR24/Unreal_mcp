#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMediaPlaybackInternal.h"

#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"
#include "McpAutomationBridgeSubsystem.h"

#if MCP_HAS_MEDIA_ASSETS
#include "Containers/Ticker.h"
#include "MediaPlayer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#endif

namespace McpSequenceMedia {

#if MCP_HAS_MEDIA_ASSETS
namespace {

constexpr float MediaOpenPollSeconds = 0.02f;
constexpr float MediaOpenTimeoutSeconds = 10.0f;
TMap<TWeakObjectPtr<UMediaPlayer>, uint64> ActivePlaybackGenerations;
uint64 NextPlaybackGeneration = 0;

struct FMediaPlaybackWaitState {
  bool bCompleted = false;
  bool bPlayRequested = false;
  float ElapsedSeconds = 0.0f;
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> Subsystem;
  TWeakObjectPtr<UMediaPlayer> Player;
  FString RequestId;
  TSharedPtr<FMcpBridgeWebSocket> Socket;
  TSharedPtr<FJsonObject> Result;
  FString ExpectedUrl;
  uint64 Generation = 0;
  FTSTicker::FDelegateHandle TickerHandle;
};

bool OwnsPlayback(const TSharedRef<FMediaPlaybackWaitState> &State) {
  const uint64 *Generation = ActivePlaybackGenerations.Find(State->Player);
  return Generation && *Generation == State->Generation;
}

void ReleasePlayback(const TSharedRef<FMediaPlaybackWaitState> &State) {
  if (OwnsPlayback(State)) {
    ActivePlaybackGenerations.Remove(State->Player);
  }
}

void FinishMediaPlayback(TSharedRef<FMediaPlaybackWaitState> State,
                         bool bSuccess, const FString &Message,
                         const FString &ErrorCode = FString(),
                         bool bTimedOut = false) {
  if (State->bCompleted) {
    return;
  }
  State->bCompleted = true;
  ReleasePlayback(State);
  if (State->TickerHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->TickerHandle);
    State->TickerHandle = FTSTicker::FDelegateHandle();
  }

  UMediaPlayer *Player = State->Player.Get();
  State->Result->SetStringField(
      TEXT("openStatus"),
      bSuccess ? TEXT("opened") : (bTimedOut ? TEXT("timeout") : TEXT("failed")));
  State->Result->SetBoolField(TEXT("timedOut"), bTimedOut);
  State->Result->SetNumberField(
      TEXT("openWaitMs"),
      static_cast<double>(State->ElapsedSeconds) * 1000.0);
  if (Player) {
    State->Result->SetBoolField(TEXT("isReady"), Player->IsReady());
    State->Result->SetBoolField(TEXT("isPlaying"), Player->IsPlaying());
    State->Result->SetBoolField(TEXT("isPaused"), Player->IsPaused());
    State->Result->SetStringField(TEXT("currentUrl"), Player->GetUrl());
  }

  if (UMcpAutomationBridgeSubsystem *Subsystem = State->Subsystem.Get()) {
    Subsystem->SendAutomationResponse(State->Socket, State->RequestId, bSuccess,
                                      Message, State->Result, ErrorCode);
  }
}

void CancelMediaPlayback(TSharedRef<FMediaPlaybackWaitState> State) {
  if (State->bCompleted) {
    return;
  }
  State->bCompleted = true;
  ReleasePlayback(State);
  if (State->TickerHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(State->TickerHandle);
    State->TickerHandle = FTSTicker::FDelegateHandle();
  }
  if (UMediaPlayer *Player = State->Player.Get()) {
    Player->Close();
  }
}

bool AdvanceMediaPlayback(TSharedRef<FMediaPlaybackWaitState> State,
                          float DeltaSeconds) {
  if (State->bCompleted) {
    return false;
  }
  State->ElapsedSeconds += DeltaSeconds;
  if (!OwnsPlayback(State)) {
    FinishMediaPlayback(State, false,
                        TEXT("A newer request replaced this media playback"),
                        TEXT("MEDIA_PLAY_SUPERSEDED"));
    return false;
  }
  if (!State->Subsystem.IsValid()) {
    FinishMediaPlayback(State, false,
                        TEXT("The automation subsystem is unavailable"),
                        TEXT("MEDIA_SUBSYSTEM_UNAVAILABLE"));
    return false;
  }
  UMediaPlayer *Player = State->Player.Get();
  if (!Player) {
    FinishMediaPlayback(State, false, TEXT("The media player is unavailable"),
                        TEXT("MEDIA_PLAYER_UNAVAILABLE"));
    return false;
  }
  if (Player->HasError()) {
    FinishMediaPlayback(State, false,
                        TEXT("The media source failed to open"),
                        TEXT("MEDIA_OPEN_FAILED"));
    return false;
  }
  if (Player->IsReady()) {
    const FString CurrentUrl = Player->GetUrl();
    if (!State->ExpectedUrl.IsEmpty() &&
        !CurrentUrl.Equals(State->ExpectedUrl, ESearchCase::CaseSensitive)) {
      FinishMediaPlayback(State, false,
                          TEXT("The media player opened a different source"),
                          TEXT("MEDIA_SOURCE_CHANGED"));
      return false;
    }
    if (Player->IsPlaying()) {
      FinishMediaPlayback(State, true, TEXT("Media playback started"));
      return false;
    }
    if (!State->bPlayRequested) {
      if (!Player->Play()) {
        FinishMediaPlayback(
            State, false, TEXT("The media player could not start playback"),
            TEXT("MEDIA_PLAY_FAILED"));
        return false;
      }
      State->bPlayRequested = true;
      if (Player->IsPlaying()) {
        FinishMediaPlayback(State, true, TEXT("Media playback started"));
        return false;
      }
    }
  }
  if (State->ElapsedSeconds >= MediaOpenTimeoutSeconds) {
    const bool bReady = Player->IsReady();
    Player->Close();
    FinishMediaPlayback(
        State, false,
        bReady ? TEXT("Media playback did not start before timeout")
               : TEXT("The media source did not open before timeout"),
        bReady ? TEXT("MEDIA_PLAY_TIMEOUT") : TEXT("MEDIA_OPEN_TIMEOUT"), true);
    return false;
  }
  return true;
}

}
#endif

void StartMediaPlaybackAfterOpen(
    UMcpAutomationBridgeSubsystem *Subsystem, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UObject *PlayerObject,
    TSharedPtr<FJsonObject> Result, FString ExpectedUrl) {
#if MCP_HAS_MEDIA_ASSETS
  UMediaPlayer *Player = Cast<UMediaPlayer>(PlayerObject);
  if (!Player) {
    SendMediaError(Subsystem, Socket, RequestId,
                   TEXT("MEDIA_PLAYER_NOT_FOUND"),
                   TEXT("The media player is unavailable"), Result);
    return;
  }
  TSharedRef<FMediaPlaybackWaitState> State =
      MakeShared<FMediaPlaybackWaitState>();
  State->Subsystem = Subsystem;
  State->Player = Player;
  State->RequestId = MoveTemp(RequestId);
  State->Socket = MoveTemp(Socket);
  State->Result = MoveTemp(Result);
  State->ExpectedUrl = MoveTemp(ExpectedUrl);
  State->Generation = ++NextPlaybackGeneration;
  ActivePlaybackGenerations.Add(State->Player, State->Generation);
  if (!Subsystem->RegisterAutomationRequestCancellation(
          State->RequestId,
          [State]() { CancelMediaPlayback(State); })) {
    CancelMediaPlayback(State);
    return;
  }
  State->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda(
          [State](float DeltaSeconds) {
            return AdvanceMediaPlayback(State, DeltaSeconds);
          }),
      MediaOpenPollSeconds);
#else
  static_cast<void>(PlayerObject);
  SendMediaError(Subsystem, Socket, RequestId,
                 TEXT("MEDIA_FRAMEWORK_UNAVAILABLE"),
                 TEXT("MediaAssets module is unavailable"), Result);
#endif
}

bool InvalidatePendingMediaPlayback(UObject *PlayerObject, bool bClosePlayer) {
#if MCP_HAS_MEDIA_ASSETS
  if (UMediaPlayer *Player = Cast<UMediaPlayer>(PlayerObject)) {
    const bool bHadPendingPlayback =
        ActivePlaybackGenerations.Remove(Player) > 0;
    if (bClosePlayer && bHadPendingPlayback)
      Player->Close();
    return bHadPendingPlayback;
  }
#else
  static_cast<void>(PlayerObject);
  static_cast<void>(bClosePlayer);
#endif
  return false;
}

}
