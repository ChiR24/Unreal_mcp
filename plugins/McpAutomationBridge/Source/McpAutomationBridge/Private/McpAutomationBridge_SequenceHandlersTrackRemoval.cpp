#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveTrack(
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

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence || !Sequence->GetMovieScene()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  bool bRemoved = false;
  FString RemovedTrackName;

  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track && Track->GetName().Contains(TrackName)) {
      RemovedTrackName = Track->GetName();
      MovieScene->RemoveTrack(*Track);
      bRemoved = true;
      break;
    }
  }

  if (!bRemoved) {
    for (const FMovieSceneBinding &Binding :
         const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
      for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
        if (Track && Track->GetName().Contains(TrackName)) {
          RemovedTrackName = Track->GetName();
          MovieScene->RemoveTrack(*Track);
          bRemoved = true;
          break;
        }
      }
      if (bRemoved)
        break;
    }
  }

  if (bRemoved) {
    MovieScene->Modify();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), RemovedTrackName);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Track removed"),
                           Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_track requires editor build"),
                         nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}
