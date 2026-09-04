#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetViewRange(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Start = 0;
  double End = 10;
  Payload->TryGetNumberField(TEXT("start"), Start);
  Payload->TryGetNumberField(TEXT("end"), End);
  FString SeqPath = ResolveSequencePath(Payload);

  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    Sequence->GetMovieScene()->SetViewRange(Start, End);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("View range set"),
                           nullptr);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
#else
  return false;
#endif
}

namespace McpSequenceRanges {
bool HandleSetWorkRange(UMcpAutomationBridgeSubsystem *Subsystem,
                        const FString &RequestId,
                        const TSharedPtr<FJsonObject> &LocalPayload,
                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_set_work_range requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("Level sequence not found"), nullptr,
                                      TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  if (!MovieScene) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("MovieScene not available"), nullptr,
                                      TEXT("MOVIESCENE_UNAVAILABLE"));
    return true;
  }

  double Start = 0.0, End = 0.0;
  LocalPayload->TryGetNumberField(TEXT("start"), Start);
  LocalPayload->TryGetNumberField(TEXT("end"), End);

  FFrameRate TickResolution = MovieScene->GetTickResolution();
  FFrameNumber StartFrame;
  FFrameNumber EndFrame;
  FString FrameError;
  if (!McpSequenceFrameMath::TrySecondsToFrame(
          Start, TickResolution, StartFrame, FrameError) ||
      !McpSequenceFrameMath::TrySecondsToFrame(
          End, TickResolution, EndFrame, FrameError)) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false, FrameError, nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MovieScene->SetWorkingRange(Start, End);

  // If the requested end lies beyond the current playback end, extend the
  // playback range to match. Keyframes placed past the playback end are cut
  // off at runtime, so a work range that reveals them must widen playback too
  // (observed: keyframes at frames beyond the default playback end never played).
  TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
  if (PlaybackRange.HasUpperBound() && EndFrame > PlaybackRange.GetUpperBoundValue())
  {
    // Upper bound is exclusive: EndFrame + 1 keeps a key on EndFrame playable.
    MovieScene->SetPlaybackRange(
        TRange<FFrameNumber>(PlaybackRange.GetLowerBoundValue(), EndFrame + 1));
  }

  MovieScene->Modify();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("startFrame"), StartFrame.Value);
  Resp->SetNumberField(TEXT("endFrame"), EndFrame.Value);
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Work range set successfully"), Resp,
                                    FString());
  return true;
#else
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("sequence_set_work_range requires editor build"), nullptr,
      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
}
