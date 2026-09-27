#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetViewRange(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  double Start = 0;
  double End = 10;
  Payload->TryGetNumberField(TEXT("start"), Start);
  Payload->TryGetNumberField(TEXT("end"), End);
  UMovieScene *MovieScene = nullptr;
  if (!McpSequence::LoadOrReply(this, RequestId, Socket, Payload, TEXT("set_view_range"), MovieScene)) {
    return true;
  }
  MovieScene->SetViewRange(Start, End);
  MovieScene->Modify();
  SendAutomationResponse(Socket, RequestId, true, TEXT("View range set"), nullptr);
  return true;
}

namespace McpSequenceRanges {
bool HandleSetWorkRange(UMcpAutomationBridgeSubsystem *Subsystem,
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
}
}
