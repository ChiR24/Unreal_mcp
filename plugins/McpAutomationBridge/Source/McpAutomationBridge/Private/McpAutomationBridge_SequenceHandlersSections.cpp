#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSection(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_section requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  double StartFrame = 0.0, EndFrame = 100.0;
  Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
  Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  UMovieSceneTrack *Track = nullptr;

  for (UMovieSceneTrack *MasterTrack : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (MasterTrack &&
        (MasterTrack->GetName().Contains(TrackName) ||
         MasterTrack->GetDisplayName().ToString().Contains(TrackName))) {
      Track = MasterTrack;
      break;
    }
  }

  if (!Track) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      if (ActorName.IsEmpty() || BindingName.Contains(ActorName)) {
        for (UMovieSceneTrack *BindingTrack : MCP_GET_BINDING_TRACKS(Binding)) {
          if (BindingTrack &&
              (BindingTrack->GetName().Contains(TrackName) ||
               BindingTrack->GetDisplayName().ToString().Contains(TrackName))) {
            Track = BindingTrack;
            break;
          }
        }
        if (Track)
          break;
      }
    }
  }

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  UMovieSceneSection *NewSection = Track->CreateNewSection();
  if (NewSection) {
    FFrameNumber Start((int32)FMath::RoundToInt(StartFrame));
    FFrameNumber End((int32)FMath::RoundToInt(EndFrame));
    NewSection->SetRange(TRange<FFrameNumber>(Start, End));
    Track->AddSection(*NewSection);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), Track->GetName());
    Resp->SetNumberField(TEXT("startFrame"), StartFrame);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Section added to track"), Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create section"), nullptr,
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_section requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}
