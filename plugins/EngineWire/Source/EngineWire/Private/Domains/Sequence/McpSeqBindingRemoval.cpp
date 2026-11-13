#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"

bool UEngineWireSubsystem::HandleSequenceRemoveActors(
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
  FString SeqPath;
  UObject *SeqObj = McpSequence::LoadAssetOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_remove_actors"), SeqPath);
  if (!SeqObj) {
    return true;
  }
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
        const FGuid RemovedGuid =
            McpSequenceKeyframes::ResolveBindingGuid(MovieScene, FString(), Name);
        const bool bRemoved = RemovedGuid.IsValid();
        if (bRemoved) {
          // The sequence's own object-binding map outlives RemovePossessable,
          // and without MarkPackageDirty the removal never reached disk and
          // the editor never offered to save it -- the mirror image of what
          // sequence_add_actors already does.
          LevelSeq->Modify();
          MovieScene->Modify();
          LevelSeq->UnbindPossessableObjects(RemovedGuid);
          MovieScene->RemovePossessable(RemovedGuid);
          LevelSeq->MarkPackageDirty();
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
  Out->SetNumberField(TEXT("removed"), RemovedCount);
  Out->SetNumberField(TEXT("total"), Arr->Num());
  // "Actors processed for removal" read identically whether three bindings
  // went or none did; sequence_add_actors was already given real counts.
  const bool bAnyRemoved = RemovedCount > 0;
  SendAutomationResponse(Socket, RequestId, bAnyRemoved,
      FString::Printf(TEXT("%d of %d binding(s) removed from the sequence"),
                      RemovedCount, Arr->Num()),
      Out, bAnyRemoved ? FString() : TEXT("NO_ACTORS_REMOVED"));
  return true;
}
