#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

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
      double FrameRateValue = 0.0;
      double LengthInFramesValue = 0.0;
      double PlaybackStartValue = 0.0;
      double PlaybackEndValue = 0.0;

      const bool bHasFrameRate =
          LocalPayload->TryGetNumberField(TEXT("frameRate"), FrameRateValue);
      const bool bHasLengthInFrames = LocalPayload->TryGetNumberField(
          TEXT("lengthInFrames"), LengthInFramesValue);
      const bool bHasPlaybackStart = LocalPayload->TryGetNumberField(
          TEXT("playbackStart"), PlaybackStartValue);
      const bool bHasPlaybackEnd = LocalPayload->TryGetNumberField(
          TEXT("playbackEnd"), PlaybackEndValue);

      if (bHasFrameRate) {
        if (FrameRateValue <= 0.0) {
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                            TEXT("frameRate must be > 0"),
                                            nullptr, TEXT("INVALID_ARGUMENT"));
          return true;
        }
        const int32 Rounded =
            FMath::Clamp<int32>(FMath::RoundToInt(FrameRateValue), 1, 960);
        FFrameRate CurrentRate = MovieScene->GetDisplayRate();
        FFrameRate NewRate(Rounded, 1);
        if (NewRate != CurrentRate) {
          MovieScene->SetDisplayRate(NewRate);
          bModified = true;
        }
      }

      if (bHasPlaybackStart || bHasPlaybackEnd || bHasLengthInFrames) {
        TRange<FFrameNumber> ExistingRange = MovieScene->GetPlaybackRange();
        FFrameNumber StartFrame = ExistingRange.GetLowerBoundValue();
        FFrameNumber EndFrame = ExistingRange.GetUpperBoundValue();

        if (bHasPlaybackStart)
          StartFrame = FFrameNumber(static_cast<int32>(PlaybackStartValue));
        if (bHasPlaybackEnd)
          EndFrame = FFrameNumber(static_cast<int32>(PlaybackEndValue));
        else if (bHasLengthInFrames)
          EndFrame =
              StartFrame +
              FMath::Max<int32>(0, static_cast<int32>(LengthInFramesValue));

        if (EndFrame < StartFrame)
          EndFrame = StartFrame;
        MovieScene->SetPlaybackRange(
            TRange<FFrameNumber>(StartFrame, EndFrame));
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
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
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
      TSharedPtr<FJsonObject> FrameRateObj =
          McpHandlerUtils::CreateResultObject();
      FrameRateObj->SetNumberField(TEXT("numerator"), FR.Numerator);
      FrameRateObj->SetNumberField(TEXT("denominator"), FR.Denominator);
      Resp->SetObjectField(TEXT("frameRate"), FrameRateObj);
      TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
      const double Start =
          static_cast<double>(Range.GetLowerBoundValue().Value);
      const double End = static_cast<double>(Range.GetUpperBoundValue().Value);
      Resp->SetNumberField(TEXT("playbackStart"), Start);
      Resp->SetNumberField(TEXT("playbackEnd"), End);
      Resp->SetNumberField(TEXT("duration"), End - Start);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("properties retrieved"), Resp, FString());
      return true;
    }
  }
  Resp->SetObjectField(TEXT("frameRate"), McpHandlerUtils::CreateResultObject());
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
