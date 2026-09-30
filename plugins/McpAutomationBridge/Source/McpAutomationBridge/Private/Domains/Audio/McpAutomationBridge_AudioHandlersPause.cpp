#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"
#include "UObject/UObjectIterator.h"

namespace McpAudioHandlers
{
// Payload: { "soundPath"?: string, "all"?: bool }. The components stop_sound stops: the 2D
// sounds play_sound started and, with all, every audio component the editor and a running
// game play (the game's music too); soundPath narrows either to one sound. SetPaused works
// in the editor and in PIE; a paused component stays active, so stop_sound still ends it.
bool HandlePauseActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const bool bPause = Lower == TEXT("pause_sound");
  if (!bPause && Lower != TEXT("resume_sound")) {
    return false;
  }
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
  bool bAll = false;
  Payload->TryGetBoolField(TEXT("all"), bAll);
  USoundBase *Only = SoundPath.IsEmpty() ? nullptr : ResolveSoundAsset(SoundPath);
  if (!SoundPath.IsEmpty() && !Only) {
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"),
                              TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // A component seen twice (a preview sound is also in the scan) is counted once: it is
  // already in the requested state on the second visit.
  int32 Changed = 0;
  const auto Apply = [&](UAudioComponent *Component) {
    if (!Component || !Component->IsPlaying() || (Only && Component->Sound != Only)) {
      return;
    }
    if ((Component->GetPlayState() == EAudioComponentPlayState::Paused) != bPause) {
      Component->SetPaused(bPause);
      ++Changed;
    }
  };
  for (const TWeakObjectPtr<UAudioComponent> &Weak : McpPreviewSounds()) {
    Apply(Weak.Get());
  }
  if (bAll) {
    for (TObjectIterator<UAudioComponent> It; It; ++It) {
      Apply(*It);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(bPause ? TEXT("paused") : TEXT("resumed"), Changed);
  McpHandlerUtils::MarkNoAssetsChanged(Resp); // pausing a sound changes no asset
  FString Message = FString::Printf(TEXT("%s %d sound(s)"), bPause ? TEXT("Paused") : TEXT("Resumed"), Changed);
  if (Changed == 0 && !bAll) {
    Message += TEXT("; only the sounds play_sound started were checked, all: true covers every sound the editor and a running game play");
  }
  Self->SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
  return true;
}
}
