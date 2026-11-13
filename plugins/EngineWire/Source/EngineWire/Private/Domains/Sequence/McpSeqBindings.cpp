#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"

bool UEngineWireSubsystem::HandleSequenceAddCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = McpSequence::ResolvePathOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_add_camera"));
  if (SeqPath.IsEmpty()) {
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  // Another kind of asset at the path used to get a camera spawned into the level and a "bound" success with no binding.
  if (!Cast<ULevelSequence>(SeqObj) || !Cast<ULevelSequence>(SeqObj)->GetMovieScene()) {
    SendAutomationError(Socket, RequestId,
                        SeqObj ? TEXT("The asset at the path is not a Level Sequence; nothing was added.") : TEXT("Sequence not found"),
                        TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (GEditor) {
    FString CameraLabel;
    LocalPayload->TryGetStringField(TEXT("actorName"), CameraLabel);
    if (CameraLabel.IsEmpty()) {
      CameraLabel = TEXT("SequenceCamera");
    }
    bool bSpawnable = false;
    LocalPayload->TryGetBoolField(TEXT("spawnable"), bSpawnable);
    ULevelSequence *SpawnSeq = Cast<ULevelSequence>(SeqObj);
    if (bSpawnable) {
      // A spawnable camera is owned by the sequence and exists only while it
      // plays, so nothing is placed in the level.
      const FGuid Guid = SpawnSeq && SpawnSeq->GetMovieScene()
                             ? static_cast<UMovieSceneSequence *>(SpawnSeq)->CreateSpawnable(ACameraActor::StaticClass())
                             : FGuid();
      if (!Guid.IsValid()) {
        SendAutomationError(Socket, RequestId,
                            TEXT("Failed to add a spawnable camera"),
                            TEXT("SPAWNABLE_CREATION_FAILED"));
        return true;
      }
      if (FMovieSceneSpawnable *Spawnable = SpawnSeq->GetMovieScene()->FindSpawnable(Guid)) {
        Spawnable->SetName(CameraLabel);
      }
      SpawnSeq->MarkPackageDirty();
      Resp->SetStringField(TEXT("bindingGuid"), Guid.ToString());
      Resp->SetBoolField(TEXT("spawnable"), true);
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Spawnable camera added to sequence"), Resp,
                             FString());
      return true;
    }
    UClass *CameraClass = ACameraActor::StaticClass();
    const TUniquePtr<FMcpScopedEditorTransaction> SpawnStep = McpBeginSpawnTransaction(RequestId);
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        CameraClass, FVector::ZeroVector, FRotator::ZeroRotator,
        CameraLabel);
    if (Spawned) {
      ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj);
      const FGuid BindingGuid = McpSequenceCinematics::ResolveOrCreateBinding(LevelSeq, Spawned);
      // An unbound camera stayed in the level behind a success; it goes again and the call fails.
      if (!BindingGuid.IsValid()) {
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DestroyActor(Spawned);
        if (SpawnStep) {
          SpawnStep->Cancel(); // nothing was added: undo keeps no step for it
        }
        SendAutomationError(Socket, RequestId,
                            TEXT("The camera could not be bound to the sequence; nothing was added."),
                            TEXT("BIND_FAILED"));
        return true;
      }
      LevelSeq->MarkPackageDirty();
      Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());

      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Camera actor spawned and bound to sequence"),
                             Resp, FString());
      return true;
    }
  }
  SendAutomationError(Socket, RequestId, TEXT("Failed to add camera"),
                      TEXT("ADD_CAMERA_FAILED"));
  return true;
}

bool UEngineWireSubsystem::HandleSequenceAddActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = McpSequence::ResolvePathOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_add_actor"));
  if (SeqPath.IsEmpty()) {
    return true;
  }

  TSharedPtr<FJsonObject> ForwardPayload = McpHandlerUtils::CreateResultObject();
  ForwardPayload->SetStringField(TEXT("path"), SeqPath);
  TArray<TSharedPtr<FJsonValue>> NamesArray;
  NamesArray.Add(MakeShared<FJsonValueString>(ActorName));
  ForwardPayload->SetArrayField(TEXT("actorNames"), NamesArray);
  bool bRebind = false;
  ForwardPayload->SetBoolField(TEXT("rebind"), LocalPayload->TryGetBoolField(TEXT("rebind"), bRebind) && bRebind);
  return HandleSequenceAddActors(RequestId, ForwardPayload, Socket);
}

bool UEngineWireSubsystem::HandleSequenceAddActors(
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
  FString SeqPath = McpSequence::ResolvePathOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_add_actors"));
  if (SeqPath.IsEmpty()) {
    return true;
  }

  bool bRebind = false;
  LocalPayload->TryGetBoolField(TEXT("rebind"), bRebind);
  TArray<FString> Names;
  Names.Reserve(Arr->Num());
  for (const TSharedPtr<FJsonValue> &V : *Arr) {
    if (V.IsValid() && V->Type == EJson::String)
      Names.Add(V->AsString());
  }

  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationError(Socket, RequestId,
                                      TEXT("Sequence not found"),
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TArray<TSharedPtr<FJsonValue>> Results;
  Results.Reserve(Names.Num());
  int32 Successful = 0;
  for (const FString &Name : Names) {
    TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
    Item->SetStringField(TEXT("name"), Name);
    AActor *Found = FindActorByName(Name);

    if (!Found) {
      Item->SetBoolField(TEXT("success"), false);
      Item->SetStringField(TEXT("error"), TEXT("Actor not found"));
    } else {
      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        UMovieScene *MovieScene = LevelSeq->GetMovieScene();
        if (MovieScene) {
          LevelSeq->Modify();
          MovieScene->Modify();
          // Reuses an existing binding of the actor instead of adding a duplicate possessable; rebind first points
          // the possessable of the actor's name at it (a sequence copied with its level).
          const FGuid Rebound = bRebind ? McpSequenceCinematics::RebindPossessableByName(LevelSeq, Found) : FGuid();
          const FGuid BindingGuid = Rebound.IsValid() ? Rebound : McpSequenceCinematics::ResolveOrCreateBinding(LevelSeq, Found);
          if (BindingGuid.IsValid()) {
            LevelSeq->MarkPackageDirty();
            Item->SetBoolField(TEXT("rebound"), Rebound.IsValid());
            Item->SetBoolField(TEXT("success"), true);
            ++Successful;
            Item->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
          } else {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(
                TEXT("error"), TEXT("Failed to create possessable binding"));
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
    }
    Results.Add(MakeShared<FJsonValueObject>(Item));
  }
  TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
  Out->SetArrayField(TEXT("results"), Results);
  const int32 Failed = Results.Num() - Successful;
  Out->SetNumberField(TEXT("total"), Names.Num());
  Out->SetNumberField(TEXT("successful"), Successful);
  Out->SetNumberField(TEXT("failed"), Failed);
  // "Actors processed" was as true of three bindings as of none - two calls,
  // one real and one naming actors that do not exist, produced byte-identical
  // receipts. Say how many bound, and refuse when nothing did.
  const bool bAnyBound = Successful > 0;
  SendAutomationResponse(Socket, RequestId, bAnyBound,
      FString::Printf(TEXT("%d of %d actor(s) bound to the sequence%s"),
                      Successful, Names.Num(),
                      Failed > 0 ? *FString::Printf(TEXT("; %d failed"), Failed) : TEXT("")),
      Out, bAnyBound ? FString() : TEXT("NO_ACTORS_BOUND"));
  return true;
}
