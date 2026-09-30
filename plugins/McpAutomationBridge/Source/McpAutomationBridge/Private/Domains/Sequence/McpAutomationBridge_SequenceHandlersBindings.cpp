#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_camera requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
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
        SendAutomationResponse(Socket, RequestId, false,
                               TEXT("Failed to add a spawnable camera"), nullptr,
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
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        CameraClass, FVector::ZeroVector, FRotator::ZeroRotator,
        CameraLabel);
    if (Spawned) {
      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
          const FGuid BindingGuid = McpSequenceCinematics::ResolveOrCreateBinding(LevelSeq, Spawned);
          if (BindingGuid.IsValid()) {
            LevelSeq->MarkPackageDirty();
            Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
          }
        }
      }

      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorLabel"), Spawned->GetActorLabel());
      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Camera actor spawned and bound to sequence"),
                             Resp, FString());
      return true;
    }
  }
  SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to add camera"),
                         nullptr, TEXT("ADD_CAMERA_FAILED"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actor requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  TSharedPtr<FJsonObject> ForwardPayload = McpHandlerUtils::CreateResultObject();
  ForwardPayload->SetStringField(TEXT("path"), SeqPath);
  TArray<TSharedPtr<FJsonValue>> NamesArray;
  NamesArray.Add(MakeShared<FJsonValueString>(ActorName));
  ForwardPayload->SetArrayField(TEXT("actorNames"), NamesArray);
  return HandleSequenceAddActors(RequestId, ForwardPayload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddActors(
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
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_add_actors requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  TArray<FString> Names;
  Names.Reserve(Arr->Num());
  for (const TSharedPtr<FJsonValue> &V : *Arr) {
    if (V.IsValid() && V->Type == EJson::String)
      Names.Add(V->AsString());
  }

  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Editor not available"), nullptr,
                                      TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Names.Num());
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
            // Reuses an existing binding of the actor instead of adding a duplicate possessable.
            const FGuid BindingGuid = McpSequenceCinematics::ResolveOrCreateBinding(LevelSeq, Found);
            if (BindingGuid.IsValid()) {
              LevelSeq->MarkPackageDirty();
              Item->SetBoolField(TEXT("success"), true);
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
    int32 Successful = 0;
    int32 Failed = 0;
    for (const TSharedPtr<FJsonValue> &Val : Results) {
      if (Val.IsValid() && Val->Type == EJson::Object) {
        bool bSuccess = false;
        Val->AsObject()->TryGetBoolField(TEXT("success"), bSuccess);
        if (bSuccess)
          Successful++;
        else
          Failed++;
      }
    }
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
  SendAutomationResponse(
      Socket, RequestId, false, TEXT("EditorActorSubsystem not available"),
      nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
  return true;
}
