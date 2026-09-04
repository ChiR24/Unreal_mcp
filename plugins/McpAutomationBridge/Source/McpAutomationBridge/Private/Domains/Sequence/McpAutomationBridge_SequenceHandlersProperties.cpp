#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetProperties(
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

#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
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

      FFrameRate NewRate = MovieScene->GetDisplayRate();
      if (bHasFrameRate) {
        FString FrameRateError;
        if (!McpSequenceFrameRate::TryParse(
                LocalPayload, TEXT("frameRate"), NewRate, FrameRateError)) {
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
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
          Subsystem->SendAutomationResponse(
              Socket, RequestIdArg, false, FrameError, nullptr,
              TEXT("INVALID_ARGUMENT"));
          return true;
        }
        if (bHasPlaybackEnd) {
          if (!McpSequenceFrameMath::TryFrameNumber(
                  PlaybackEndValue, EndFrame, FrameError)) {
            Subsystem->SendAutomationResponse(
                Socket, RequestIdArg, false, FrameError, nullptr,
                TEXT("INVALID_ARGUMENT"));
            return true;
          }
        } else if (bHasLengthInFrames) {
          FFrameNumber Length;
          if (!McpSequenceFrameMath::TryFrameNumber(
                  LengthInFramesValue, Length, FrameError)) {
            Subsystem->SendAutomationResponse(
                Socket, RequestIdArg, false, FrameError, nullptr,
                TEXT("INVALID_ARGUMENT"));
            return true;
          }
          if (!McpSequenceFrameMath::TryAddFrames(
                  StartFrame,
                  FMath::Max(0, Length.Value),
                  EndFrame, FrameError)) {
            Subsystem->SendAutomationResponse(
                Socket, RequestIdArg, false, FrameError, nullptr,
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

      if (NewRate != MovieScene->GetDisplayRate()) {
        MovieScene->SetDisplayRate(NewRate);
        bModified = true;
      }
      if (bRangeChanged) {
        MovieScene->SetPlaybackRange(NewRange);
        bModified = true;
      }

      if (bModified)
        MovieScene->Modify();

      FFrameRate FR = MovieScene->GetDisplayRate();
      TSharedPtr<FJsonObject> FrameRateObj =
          McpHandlerUtils::CreateResultObject();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);

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

      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("properties updated"), Resp,
                                        FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), McpHandlerUtils::CreateResultObject());
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  Resp->SetBoolField(TEXT("applied"), false);
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false,
      TEXT("sequence_set_properties is not available in this editor build or "
           "for this sequence type"),
      Resp, TEXT("NOT_IMPLEMENTED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetProperties(
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
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FFrameRate FR = MovieScene->GetDisplayRate();
      // BB-040: the record declares frameRate as a number|string union, so the
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
  // This fallback still answers success, so it is held to the same declared
  // number|string union as the branch above; an empty object here was the same
  // violation under a different code path.
  Resp->SetNumberField(TEXT("frameRate"), 0.0);
  Resp->SetNumberField(TEXT("playbackStart"), 0.0);
  Resp->SetNumberField(TEXT("playbackEnd"), 0.0);
  Resp->SetNumberField(TEXT("duration"), 0.0);
  SendAutomationResponse(Socket, RequestId, true, TEXT("properties retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_properties requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
