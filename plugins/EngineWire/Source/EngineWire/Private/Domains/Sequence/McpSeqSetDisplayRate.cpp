#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqFrameRate.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "MovieSceneTimeHelpers.h"

namespace {
// Ticks per second as a whole number (24000) or a rational (24000/1001). "Contains 24000" read 24000/1001 and 240000
// as 24000, and zero, negative or other input kept the old resolution while the reply said it was set.
bool McpParseTickResolution(const FString &Text, FFrameRate &Out) {
  FString Num = Text.TrimStartAndEnd(), Den = TEXT("1");
  if (Num.Contains(TEXT("/")) && !Text.TrimStartAndEnd().Split(TEXT("/"), &Num, &Den))
    return false;
  auto IsWhole = [](const FString &S) {
    if (S.IsEmpty() || S.Len() > 9)
      return false;
    for (const TCHAR Ch : S)
      if (!FChar::IsDigit(Ch))
        return false;
    return true;
  };
  Num.TrimStartAndEndInline();
  Den.TrimStartAndEndInline();
  if (!IsWhole(Num) || !IsWhole(Den) || FCString::Atoi(*Num) <= 0 || FCString::Atoi(*Den) <= 0)
    return false;
  Out = FFrameRate(FCString::Atoi(*Num), FCString::Atoi(*Den));
  return true;
}
} // namespace

bool UEngineWireSubsystem::HandleSequenceSetDisplayRate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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

  UObject *SeqObj = McpLoadAsset(SeqPath);
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
        // One undo step: Modify after the change recorded nothing to undo to.
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Sequence Display Rate")),
                                                EMcpMutationDurability::EditorStateOnly, {LevelSeq, MovieScene});
        MovieScene->SetDisplayRate(NewRate);
        LevelSeq->MarkPackageDirty();
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("displayRate"),
                             MovieScene->GetDisplayRate().ToPrettyText().ToString());
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
}

bool UEngineWireSubsystem::HandleSequenceSetTickResolution(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString ResolutionStr;
  Payload->TryGetStringField(TEXT("resolution"), ResolutionStr);
  FFrameRate NewResolution;
  if (!McpParseTickResolution(ResolutionStr, NewResolution)) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("resolution must be whole ticks per second, such as 24000 or 60000 (a ratio "
                                                "N/D is N ticks every D seconds); got '%s'. Nothing was changed."), *ResolutionStr),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SeqPath = ResolveSequencePath(Payload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("path required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  // A grid coarser than the display rate cannot hold every frame: 24000/1001 (24 ticks a second) moved a section at
  // frame 24 to 23.77, and every edit after that landed between frames.
  if (Sequence && Sequence->GetMovieScene() &&
      NewResolution.AsDecimal() < Sequence->GetMovieScene()->GetDisplayRate().AsDecimal()) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("resolution %s is coarser than the display rate (%s), so frames would fall "
                                                "between ticks; use whole ticks per second such as 24000. Nothing was changed."),
                                           *ResolutionStr, *Sequence->GetMovieScene()->GetDisplayRate().ToPrettyText().ToString()),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Sequence && Sequence->GetMovieScene()) {
    UMovieScene *MovieScene = Sequence->GetMovieScene();
    const FFrameRate Previous = MovieScene->GetTickResolution();
    // Keys, sections and ranges are stored in ticks: setting the resolution alone moved every one of them in time.
    // Migrate them as the Sequencer's own tick resolution change does, which then sets the new resolution.
    {
      FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Sequence Tick Resolution")),
                                              EMcpMutationDurability::EditorStateOnly, {Sequence, MovieScene});
      UE::MovieScene::TimeHelpers::MigrateFrameTimes(Previous, NewResolution, MovieScene);
    }
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("tickResolution"), MovieScene->GetTickResolution().ToPrettyText().ToString());
    Resp->SetStringField(TEXT("previousTickResolution"), Previous.ToPrettyText().ToString());
    // A display frame that is not a whole number of ticks falls between them.
    if (!MovieScene->GetDisplayRate().IsMultipleOf(MovieScene->GetTickResolution())) {
      TArray<TSharedPtr<FJsonValue>> Warnings;
      Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("The display rate %s is not a whole number of ticks at %s, "
          "so frames fall between ticks."), *MovieScene->GetDisplayRate().ToPrettyText().ToString(),
          *MovieScene->GetTickResolution().ToPrettyText().ToString())));
      Resp->SetArrayField(TEXT("warnings"), Warnings);
    }
    McpHandlerUtils::AddVerification(Resp, Sequence);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Tick resolution set"),
                           Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("NOT_FOUND"));
  }
  return true;
}
