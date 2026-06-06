#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetDisplayRate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_display_rate requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      FString FrameRateStr;
      double FrameRateVal = 0.0;
      FFrameRate NewRate;
      bool bRateFound = false;

      if (LocalPayload->TryGetStringField(TEXT("frameRate"), FrameRateStr)) {
        if (FrameRateStr.EndsWith(TEXT("fps"))) {
          FrameRateStr.RemoveFromEnd(TEXT("fps"));
          NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
          bRateFound = true;
        } else if (FrameRateStr.Contains(TEXT("/"))) {
          FString NumStr, DenomStr;
          if (FrameRateStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
            NewRate =
                FFrameRate(FCString::Atoi(*NumStr), FCString::Atoi(*DenomStr));
            bRateFound = true;
          }
        } else {
          if (FrameRateStr.IsNumeric()) {
            NewRate = FFrameRate(FCString::Atoi(*FrameRateStr), 1);
            bRateFound = true;
          }
        }
      } else if (LocalPayload->TryGetNumberField(TEXT("frameRate"),
                                                 FrameRateVal)) {
        NewRate = FFrameRate(FMath::RoundToInt(FrameRateVal), 1);
        bRateFound = true;
      }

      if (bRateFound) {
        MovieScene->SetDisplayRate(NewRate);
        MovieScene->Modify();
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("displayRate"),
                             NewRate.ToPrettyText().ToString());
        McpHandlerUtils::AddVerification(Resp, LevelSeq);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Display rate set"), Resp, FString());
        return true;
      }

      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Invalid frameRate format"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Invalid sequence type"), nullptr,
                         TEXT("INVALID_SEQUENCE"));
  return true;
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_set_display_rate requires editor build"), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetTickResolution(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ResolutionStr;
  Payload->TryGetStringField(TEXT("resolution"), ResolutionStr);

  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (Sequence && Sequence->GetMovieScene()) {
    FFrameRate TickResolution = Sequence->GetMovieScene()->GetTickResolution();
    if (!ResolutionStr.IsEmpty()) {
      if (ResolutionStr.Contains(TEXT("24000")))
        TickResolution = FFrameRate(24000, 1);
      else if (ResolutionStr.Contains(TEXT("60000")))
        TickResolution = FFrameRate(60000, 1);
      else if (ResolutionStr.Contains(TEXT("/"))) {
        FString NumStr, DenomStr;
        if (ResolutionStr.Split(TEXT("/"), &NumStr, &DenomStr)) {
          int32 Num = FCString::Atoi(*NumStr);
          int32 Denom = FCString::Atoi(*DenomStr);
          if (Num > 0 && Denom > 0) {
            TickResolution = FFrameRate(Num, Denom);
          }
        }
      } else if (ResolutionStr.IsNumeric()) {
        int32 Num = FCString::Atoi(*ResolutionStr);
        if (Num > 0) {
          TickResolution = FFrameRate(Num, 1);
        }
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("HandleSequenceSetTickResolution: Unrecognized resolution format '%s'. Using current resolution."),
               *ResolutionStr);
      }
    }

    Sequence->GetMovieScene()->SetTickResolutionDirectly(TickResolution);
    Sequence->GetMovieScene()->Modify();
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tick resolution set"),
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
