#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqFrameRate.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"

namespace {
// Why Object at Path has no playback properties to read or set.
FString McpNotALevelSequence(const FString &Path, const UObject *Object) {
  return Cast<ULevelSequence>(Object) ? FString::Printf(TEXT("%s has no movie scene"), *Path)
                                      : FString::Printf(TEXT("%s is a %s, not a Level Sequence"), *Path, *Object->GetClass()->GetName());
}
} // namespace

bool UEngineWireSubsystem::HandleSequenceSetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      bool bModified = false;
      double LengthInFramesValue = 0.0;
      double PlaybackStartValue = 0.0;
      double PlaybackEndValue = 0.0;

      const bool bHasFrameRate = LocalPayload->HasField(TEXT("frameRate"));
      const bool bHasLengthInFrames = LocalPayload->TryGetNumberField(
          TEXT("lengthInFrames"), LengthInFramesValue);
      const bool bHasPlaybackStart = LocalPayload->TryGetNumberField(
          TEXT("playbackStart"), PlaybackStartValue);
      const bool bHasPlaybackEnd = LocalPayload->TryGetNumberField(
          TEXT("playbackEnd"), PlaybackEndValue);
      // A call that set nothing answered "properties updated".
      if (!bHasFrameRate && !bHasLengthInFrames && !bHasPlaybackStart && !bHasPlaybackEnd) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Pass frameRate, playbackStart, playbackEnd or lengthInFrames. Nothing was changed."),
                               nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
      }

      FFrameRate NewRate = MovieScene->GetDisplayRate();
      if (bHasFrameRate) {
        FString FrameRateError;
        if (!McpSequenceFrameRate::TryParse(
                LocalPayload, TEXT("frameRate"), NewRate, FrameRateError)) {
          SendAutomationResponse(Socket, RequestId, false,
                                            FrameRateError,
                                            nullptr, TEXT("INVALID_ARGUMENT"));
          return true;
        }
      }

      TRange<FFrameNumber> NewRange = MovieScene->GetPlaybackRange();
      bool bRangeChanged = false;
      if (bHasPlaybackStart || bHasPlaybackEnd || bHasLengthInFrames) {
        // Work in display-rate frames (the contract's unit); the stored range
        // is in tick resolution, so convert on the way in and out.
        const FFrameRate TickRate = MovieScene->GetTickResolution();
        FFrameNumber StartFrame = FFrameRate::TransformTime(FFrameTime(NewRange.GetLowerBoundValue()), TickRate, NewRate).FloorToFrame();
        FFrameNumber EndFrame = FFrameRate::TransformTime(FFrameTime(NewRange.GetUpperBoundValue()), TickRate, NewRate).FloorToFrame();

        FString FrameError;
        if (bHasPlaybackStart &&
            !McpSequenceFrameMath::TryFrameNumber(
                PlaybackStartValue, StartFrame, FrameError)) {
          SendAutomationResponse(
              Socket, RequestId, false, FrameError, nullptr,
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        if (bHasPlaybackEnd) {
          if (!McpSequenceFrameMath::TryFrameNumber(
                  PlaybackEndValue, EndFrame, FrameError)) {
            SendAutomationResponse(
                Socket, RequestId, false, FrameError, nullptr,
                TEXT("INVALID_ARGUMENT"));
            return true;
          }
        } else if (bHasLengthInFrames) {
          FFrameNumber Length;
          if (!McpSequenceFrameMath::TryFrameNumber(
                  LengthInFramesValue, Length, FrameError)) {
            SendAutomationResponse(
                Socket, RequestId, false, FrameError, nullptr,
                TEXT("INVALID_ARGUMENT"));
            return true;
          }
          if (!McpSequenceFrameMath::TryAddFrames(
                  StartFrame,
                  FMath::Max(0, Length.Value),
                  EndFrame, FrameError)) {
            SendAutomationResponse(
                Socket, RequestId, false, FrameError, nullptr,
                TEXT("INVALID_ARGUMENT"));
            return true;
          }
        }

        if (EndFrame < StartFrame)
          EndFrame = StartFrame;
        NewRange = TRange<FFrameNumber>(
            FFrameRate::TransformTime(FFrameTime(StartFrame), NewRate, TickRate).FloorToFrame(),
            FFrameRate::TransformTime(FFrameTime(EndFrame), NewRate, TickRate).FloorToFrame());
        bRangeChanged = true;
      }

      // Modify before the change, so undo restores the old values (after it, undo held the new ones).
      bModified = NewRate != MovieScene->GetDisplayRate() ||
                  (bRangeChanged && NewRange != MovieScene->GetPlaybackRange());
      if (bModified)
        MovieScene->Modify();
      if (NewRate != MovieScene->GetDisplayRate())
        MovieScene->SetDisplayRate(NewRate);
      if (bRangeChanged)
        MovieScene->SetPlaybackRange(NewRange);

      // A number, as get_properties gives it.
      FFrameRate FR = MovieScene->GetDisplayRate();
      Resp->SetNumberField(TEXT("frameRate"), FR.AsDecimal());

      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      // Playback range is stored in tick resolution; publish display-rate frames
      // (the unit the contract names) instead of raw ticks.
      const FFrameRate TickRate = MovieScene->GetTickResolution();
      const double Start = FFrameRate::TransformTime(FFrameTime(Range.GetLowerBoundValue()), TickRate, FR).FloorToFrame().Value;
      const double End = FFrameRate::TransformTime(FFrameTime(Range.GetUpperBoundValue()), TickRate, FR).FloorToFrame().Value;
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      Resp->SetNumberField(TEXT("lengthInFrames"), End - Start);
      Resp->SetBoolField(TEXT("applied"), bModified);

      SendAutomationResponse(Socket, RequestId, true,
                             bModified ? TEXT("Sequence properties set; the sequence is unsaved")
                                       : TEXT("Sequence properties already had those values"),
                             Resp, FString());
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false, McpNotALevelSequence(SeqPath, SeqObj), nullptr,
                         TEXT("INVALID_SEQUENCE"));
  return true;
}

bool UEngineWireSubsystem::HandleSequenceGetProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_properties requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FFrameRate FR = MovieScene->GetDisplayRate();
      // The record declares frameRate as a number|string union, so the
      // {numerator,denominator} object this used to emit failed output
      // validation. get_properties only; the set_properties site is unchanged.
      Resp->SetNumberField(TEXT("frameRate"), FR.AsDecimal());
      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      // Playback range is stored in tick resolution; publish display-rate frames
      // (the unit the contract names) instead of raw ticks.
      const FFrameRate TickRate = MovieScene->GetTickResolution();
      const double Start = FFrameRate::TransformTime(FFrameTime(Range.GetLowerBoundValue()), TickRate, FR).FloorToFrame().Value;
      const double End = FFrameRate::TransformTime(FFrameTime(Range.GetUpperBoundValue()), TickRate, FR).FloorToFrame().Value;
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      Resp->SetNumberField(TEXT("lengthInFrames"), End - Start);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("properties retrieved"), Resp, FString());
      return true;
    }
  }
  // Any other asset answered success with every value 0, the same as a real sequence at 0.
  SendAutomationResponse(Socket, RequestId, false, McpNotALevelSequence(SeqPath, SeqObj), nullptr,
                         TEXT("INVALID_SEQUENCE"));
  return true;
}
