#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"

bool UEngineWireSubsystem::HandleSequenceSetViewRange(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  double Start = 0;
  double End = 0;
  // Both are seconds; a missing one defaulted (0 to 10 s) and the reply said the range was set.
  if (!Payload->TryGetNumberField(TEXT("start"), Start) || !Payload->TryGetNumberField(TEXT("end"), End) || End <= Start) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("start and end are required, in seconds, with end after start"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UMovieScene *MovieScene = nullptr;
  if (!McpSequence::LoadOrReply(this, RequestId, Socket, Payload, TEXT("set_view_range"), MovieScene)) {
    return true;
  }
  MovieScene->Modify();
  MovieScene->SetViewRange(Start, End);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("start"), Start);
  Resp->SetNumberField(TEXT("end"), End);
  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("View range %g to %g s"), Start, End), Resp);
  return true;
}

namespace McpSequenceRanges {
bool HandleSetWorkRange(UEngineWireSubsystem *Subsystem,
                        const FString &RequestId,
                        const TSharedPtr<FJsonObject> &LocalPayload,
                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(Subsystem, RequestId, RequestingSocket, LocalPayload, TEXT("set_work_range"), MovieScene);
  if (!Sequence) {
    return true;
  }
  const FString SeqPath = Sequence->GetPathName();

  double Start = 0.0, End = 0.0;
  LocalPayload->TryGetNumberField(TEXT("start"), Start);
  LocalPayload->TryGetNumberField(TEXT("end"), End);
  if (End <= Start) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("start and end are seconds, with end after start"), nullptr,
                                      TEXT("INVALID_ARGUMENT"));
    return true;
  }

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

  // Before the change, so undo restores the old range.
  MovieScene->Modify();
  MovieScene->SetWorkingRange(Start, End);

  // If the requested end lies beyond the current playback end, extend the
  // playback range to match. Keyframes placed past the playback end are cut
  // off at runtime, so a work range that reveals them must widen playback too
  // (observed: keyframes at frames beyond the default playback end never played).
  TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
  const bool bPlaybackExtended = PlaybackRange.HasUpperBound() && EndFrame > PlaybackRange.GetUpperBoundValue();
  if (bPlaybackExtended)
  {
    // Upper bound is exclusive: EndFrame + 1 keeps a key on EndFrame playable.
    MovieScene->SetPlaybackRange(
        TRange<FFrameNumber>(PlaybackRange.GetLowerBoundValue(), EndFrame + 1));
  }

  // Display-rate frames, the unit the timeline shows; the tick-resolution numbers were thousands of times larger.
  const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("start"), Start);
  Resp->SetNumberField(TEXT("end"), End);
  Resp->SetNumberField(TEXT("startFrame"), FFrameRate::TransformTime(FFrameTime(StartFrame), TickResolution, DisplayRate).FloorToFrame().Value);
  Resp->SetNumberField(TEXT("endFrame"), FFrameRate::TransformTime(FFrameTime(EndFrame), TickResolution, DisplayRate).FloorToFrame().Value);
  Resp->SetBoolField(TEXT("playbackExtended"), bPlaybackExtended);
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Work range set successfully"), Resp,
                                    FString());
  return true;
}
}
