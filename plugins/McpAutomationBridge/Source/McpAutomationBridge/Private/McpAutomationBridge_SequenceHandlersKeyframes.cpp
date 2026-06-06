#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace {
bool NormalizeSequenceTransformAlias(const TSharedPtr<FJsonObject> &Payload,
                                     FString &PropertyName,
                                     const TCHAR *Alias,
                                     const TCHAR *TransformField) {
  if (!PropertyName.Equals(Alias, ESearchCase::IgnoreCase)) {
    return false;
  }

  const TSharedPtr<FJsonObject> *ValueObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("value"), ValueObj) && ValueObj &&
      ValueObj->IsValid()) {
    TSharedPtr<FJsonObject> TransformValue = MakeShared<FJsonObject>();
    TransformValue->SetObjectField(TransformField, *ValueObj);
    Payload->SetObjectField(TEXT("value"), TransformValue);
  }

  PropertyName = TEXT("Transform");
  return true;
}
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddKeyframe(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_keyframe requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  FString BindingIdStr;
  LocalPayload->TryGetStringField(TEXT("bindingId"), BindingIdStr);
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  FString PropertyName;
  LocalPayload->TryGetStringField(TEXT("property"), PropertyName);

  if (BindingIdStr.IsEmpty() && ActorName.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Either bindingId or actorName must be provided. bindingId is the "
             "GUID from add_actor/get_bindings. actorName is the label of an "
             "actor already bound to the sequence. Example: {\"actorName\": "
             "\"MySphere\", \"property\": \"Location\", \"frame\": 0, "
             "\"value\": {\"x\":0,\"y\":0,\"z\":0}}"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double Frame = 0.0;
  if (!LocalPayload->TryGetNumberField(TEXT("frame"), Frame)) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("frame number is required. Example: "
                                "{\"frame\": 30} for keyframe at frame 30"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  NormalizeSequenceTransformAlias(LocalPayload, PropertyName, TEXT("Location"),
                                  TEXT("location")) ||
      NormalizeSequenceTransformAlias(LocalPayload, PropertyName,
                                      TEXT("Translation"), TEXT("location")) ||
      NormalizeSequenceTransformAlias(LocalPayload, PropertyName,
                                      TEXT("Rotation"), TEXT("rotation")) ||
      NormalizeSequenceTransformAlias(LocalPayload, PropertyName, TEXT("Scale"),
                                      TEXT("scale"));

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      FGuid BindingGuid =
          McpSequenceKeyframes::ResolveBindingGuid(MovieScene, BindingIdStr,
                                                   ActorName);
      if (!BindingGuid.IsValid()) {
        FString Target = !BindingIdStr.IsEmpty() ? BindingIdStr : ActorName;
        SendAutomationResponse(
            Socket, RequestId, false,
            FString::Printf(TEXT("Binding not found for '%s'. Ensure actor is "
                                 "bound to sequence."),
                            *Target),
            nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      FMovieSceneBinding *Binding = MovieScene->FindBinding(BindingGuid);
      if (!Binding) {
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Binding object not found in sequence"),
                               nullptr, TEXT("BINDING_NOT_FOUND"));
        return true;
      }

      if (PropertyName.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)) {
        if (McpSequenceKeyframes::AddTransformKeyframe(
                MovieScene, BindingGuid, Frame, LocalPayload)) {
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Keyframe added"), nullptr, FString());
          return true;
        }
      } else {
        FString SuccessMessage;
        if (McpSequenceKeyframes::AddPropertyKeyframe(
                MovieScene, BindingGuid, PropertyName, Frame, LocalPayload,
                SuccessMessage)) {
          SendAutomationResponse(Socket, RequestId, true, SuccessMessage,
                                 nullptr);
          return true;
        }
      }

      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Unsupported property or failed to create track"), nullptr,
          TEXT("UNSUPPORTED_PROPERTY"));
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_keyframe requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
