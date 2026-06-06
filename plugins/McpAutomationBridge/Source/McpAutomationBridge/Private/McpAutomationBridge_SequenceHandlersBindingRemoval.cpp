#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceRemoveActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorNames required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_remove_actors requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Removed;
    int32 RemovedCount = 0;
    for (const TSharedPtr<FJsonValue> &V : *Arr) {
      if (!V.IsValid() || V->Type != EJson::String)
        continue;
      FString Name = V->AsString();
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("name"), Name);

      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        UMovieScene *MovieScene = LevelSeq->GetMovieScene();
        if (MovieScene) {
          bool bRemoved = false;
          for (const FMovieSceneBinding &Binding :
               const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
            FString BindingName;
            if (FMovieScenePossessable *Possessable =
                    MovieScene->FindPossessable(Binding.GetObjectGuid())) {
              BindingName = Possessable->GetName();
            } else if (FMovieSceneSpawnable *Spawnable =
                           MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
              BindingName = Spawnable->GetName();
            }

            if (BindingName.Equals(Name, ESearchCase::IgnoreCase)) {
              MovieScene->RemovePossessable(Binding.GetObjectGuid());
              MovieScene->Modify();
              bRemoved = true;
              break;
            }
          }
          if (bRemoved) {
            Item->SetBoolField(TEXT("success"), true);
            Item->SetStringField(TEXT("status"), TEXT("Actor removed"));
            RemovedCount++;
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"),
                                 TEXT("Actor not found in sequence bindings"));
          }
        } else {
          Item->SetBoolField(TEXT("success"), false);
          Item->SetStringField(TEXT("error"),
                               TEXT("Sequence has no MovieScene"));
        }
      } else {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"),
                             TEXT("Sequence object is not a LevelSequence"));
      }
      Removed.Add(MakeShared<FJsonValueObject>(Item));
    }
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetArrayField(TEXT("removedActors"), Removed);
    Out->SetNumberField(TEXT("bindingsProcessed"), RemovedCount);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actors processed for removal"), Out,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("EditorActorSubsystem not available"), nullptr,
                         TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_remove_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
