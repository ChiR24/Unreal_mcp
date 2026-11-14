#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"

// Removes the possessable Guid names and, first, every one under it (a component binding hangs under its actor's),
// as Sequencer's delete does: a child left behind kept its tracks under a parent that was gone. Returns how many
// children went.
static int32 RemovePossessableTree(ULevelSequence *Sequence, UMovieScene *MovieScene, const FGuid &Guid) {
  TArray<FGuid> Children;
  for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index) {
    if (MovieScene->GetPossessable(Index).GetParent() == Guid) {
      Children.Add(MovieScene->GetPossessable(Index).GetGuid());
    }
  }
  int32 Removed = 0;
  for (const FGuid &Child : Children) {
    Removed += 1 + RemovePossessableTree(Sequence, MovieScene, Child);
  }
  Sequence->UnbindPossessableObjects(Guid);
  MovieScene->RemovePossessable(Guid);
  return Removed;
}

bool UEngineWireSubsystem::HandleSequenceRemoveActors(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  LocalPayload->TryGetArrayField(TEXT("actorNames"), Arr);
  if (!Arr || Arr->Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("actorNames required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *LevelSeq = McpSequence::LoadOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_remove_actors"), MovieScene);
  if (!LevelSeq) {
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

    const FGuid RemovedGuid = McpSequenceKeyframes::ResolveBindingGuid(MovieScene, FString(), Name);
    if (RemovedGuid.IsValid()) {
      // The sequence's own object-binding map outlives RemovePossessable,
      // and without MarkPackageDirty the removal never reached disk and
      // the editor never offered to save it -- the mirror image of what
      // sequence_add_actors already does.
      LevelSeq->Modify();
      MovieScene->Modify();
      const int32 Children = RemovePossessableTree(LevelSeq, MovieScene, RemovedGuid);
      LevelSeq->MarkPackageDirty();
      Item->SetBoolField(TEXT("success"), true);
      Item->SetStringField(TEXT("status"), TEXT("Actor removed"));
      Item->SetNumberField(TEXT("childBindingsRemoved"), Children);
      RemovedCount++;
    } else {
      Item->SetBoolField(TEXT("success"), false);
      Item->SetStringField(TEXT("error"), TEXT("Actor not found in sequence bindings"));
    }
    Removed.Add(MakeShared<FJsonValueObject>(Item));
  }
  TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
  Out->SetArrayField(TEXT("removedActors"), Removed);
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
