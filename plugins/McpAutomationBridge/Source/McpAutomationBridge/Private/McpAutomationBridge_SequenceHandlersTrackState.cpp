#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackMuted(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bMuted = true;
  Payload->TryGetBoolField(TEXT("muted"), bMuted);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  Track->SetEvalDisabled(bMuted);
  MovieScene->Modify();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), Track->GetName());
  Resp->SetBoolField(TEXT("muted"), bMuted);
  SendAutomationResponse(Socket, RequestId, true,
                         bMuted ? TEXT("Track muted") : TEXT("Track unmuted"),
                         Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_muted requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackSolo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bSolo = true;
  Payload->TryGetBoolField(TEXT("solo"), bSolo);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *SoloTrack = nullptr;
  TArray<UMovieSceneTrack *> AllTracks;
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track) {
      AllTracks.Add(Track);
      if (Track->GetName().Contains(TrackName)) {
        SoloTrack = Track;
      }
    }
  }

  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (Track) {
        AllTracks.Add(Track);
        if (Track->GetName().Contains(TrackName)) {
          SoloTrack = Track;
        }
      }
    }
  }

  if (!SoloTrack) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  int32 AffectedTrackCount = 0;
  int32 DisabledOtherTrackCount = 0;
  for (UMovieSceneTrack *Track : AllTracks) {
    if (!Track) {
      continue;
    }
    if (bSolo) {
      const bool bDisableTrack = Track != SoloTrack;
      Track->SetEvalDisabled(bDisableTrack);
      if (bDisableTrack) {
        ++DisabledOtherTrackCount;
      }
    } else {
      Track->SetEvalDisabled(false);
    }
    ++AffectedTrackCount;
  }
  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("trackName"), SoloTrack->GetName());
  Resp->SetBoolField(TEXT("solo"), bSolo);
  Resp->SetNumberField(TEXT("affectedTrackCount"), AffectedTrackCount);
  Resp->SetNumberField(TEXT("disabledOtherTrackCount"), DisabledOtherTrackCount);
  SendAutomationResponse(
      Socket, RequestId, true,
      bSolo ? TEXT("Track solo enabled by disabling evaluation on other tracks")
            : TEXT("Solo disabled; all tracks evaluation-enabled"),
      Resp);
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_track_solo requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTrackLocked(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence path required"), nullptr,
                           TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  bool bLocked = true;
  Payload->TryGetBoolField(TEXT("locked"), bLocked);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;
  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack && MasterTrack->GetName().Contains(TrackName)) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
        if (BindingTrack && BindingTrack->GetName().Contains(TrackName)) {
          Track = BindingTrack;
          break;
        }
      }
      if (Track)
        break;
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

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
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_track_locked requires editor build"), nullptr,
      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
