#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

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

  if (UMovieSceneTrack *Track = FindTrackByName(MovieScene, TrackName)) {
    RemovedTrackName = Track->GetName();
    MovieScene->RemoveTrack(*Track);
    bRemoved = true;
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
