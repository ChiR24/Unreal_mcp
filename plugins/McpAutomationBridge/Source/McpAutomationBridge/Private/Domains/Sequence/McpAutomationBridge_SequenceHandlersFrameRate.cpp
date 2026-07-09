#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

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
      FFrameRate NewRate;
      FString FrameRateError;
      if (McpSequenceFrameRate::TryParse(
              LocalPayload, TEXT("frameRate"), NewRate, FrameRateError)) {
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
                             FrameRateError, nullptr,
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
