#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace {
// The payload's sequence and its trackName track; replies and returns null on a miss.
UMovieSceneTrack *LoadTrackOrReply(UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
                                   TSharedPtr<FMcpBridgeWebSocket> Socket,
                                   const TSharedPtr<FJsonObject> &Payload, const TCHAR *Action,
                                   ULevelSequence *&OutSequence, UMovieScene *&OutMovieScene) {
  OutSequence = McpSequence::LoadOrReply(Self, RequestId, Socket, Payload, Action, OutMovieScene);
  if (!OutSequence)
    return nullptr;
  UMovieSceneTrack *Track =
      FindTrackByName(OutMovieScene, GetJsonStringField(Payload, TEXT("trackName")));
  if (!Track)
    Self->SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"), nullptr,
                                 TEXT("TRACK_NOT_FOUND"));
  return Track;
}
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackMuted(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_muted"), Sequence, MovieScene);
  if (!Track)
    return true;
  const bool bMuted = GetJsonBoolField(Payload, TEXT("muted"), true);
  Track->SetEvalDisabled(bMuted);
  MovieScene->Modify();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("muted"), bMuted);
  SendAutomationResponse(Socket, RequestId, true,
                         bMuted ? TEXT("Track muted") : TEXT("Track unmuted"), Resp);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackSolo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *SoloTrack = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_solo"), Sequence, MovieScene);
  if (!SoloTrack)
    return true;
  const bool bSolo = GetJsonBoolField(Payload, TEXT("solo"), true);
  TArray<UMovieSceneTrack *> AllTracks;
  CollectTracksByName(MovieScene, FString(), FString(), AllTracks);
  int32 DisabledOtherTrackCount = 0;
  for (UMovieSceneTrack *Track : AllTracks) {
    const bool bDisableTrack = bSolo && Track != SoloTrack;
    Track->SetEvalDisabled(bDisableTrack);
    DisabledOtherTrackCount += bDisableTrack ? 1 : 0;
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), SoloTrack->GetName());
  Resp->SetBoolField(TEXT("solo"), bSolo);
  Resp->SetNumberField(TEXT("affectedTrackCount"), AllTracks.Num());
  Resp->SetNumberField(TEXT("disabledOtherTrackCount"), DisabledOtherTrackCount);
  SendAutomationResponse(
      Socket, RequestId, true,
      bSolo ? TEXT("Track solo enabled by disabling evaluation on other tracks")
            : TEXT("Solo disabled; all tracks evaluation-enabled"),
      Resp);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackLocked(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("set_track_locked"), Sequence, MovieScene);
  if (!Track)
    return true;
  const bool bLocked = GetJsonBoolField(Payload, TEXT("locked"), true);
  for (UMovieSceneSection *Section : Track->GetAllSections()) {
    if (Section) {
      Section->SetIsLocked(bLocked);
    }
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("locked"), bLocked);
  SendAutomationResponse(
      Socket, RequestId, true,
      bLocked ? TEXT("Track locked") : TEXT("Track unlocked"), Resp);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveTrack(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  ULevelSequence *Sequence = nullptr;
  UMovieScene *MovieScene = nullptr;
  UMovieSceneTrack *Track = LoadTrackOrReply(this, RequestId, Socket, Payload, TEXT("remove_track"), Sequence, MovieScene);
  if (!Track)
    return true;
  const FString RemovedTrackName = Track->GetName();
  // Modify() has to precede the mutation or the transaction records the
  // post-change state and undo cannot bring the track back. MarkPackageDirty
  // is what makes the removal reach disk at all: without it the editor never
  // even offers to save, so a restart resurrected every removed track --
  // the same defect sequence_remove_actor already documents.
  Sequence->Modify();
  MovieScene->Modify();
  // RemoveTrack only searches the Tracks array, so it silently fails on the
  // camera cut track, which lives in its own member and needs its own call.
  if (Track == MovieScene->GetCameraCutTrack()) {
    MovieScene->RemoveCameraCutTrack();
  } else {
    MovieScene->RemoveTrack(*Track);
  }
  Sequence->MarkPackageDirty();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), RemovedTrackName);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Track removed"), Resp);
  return true;
}
