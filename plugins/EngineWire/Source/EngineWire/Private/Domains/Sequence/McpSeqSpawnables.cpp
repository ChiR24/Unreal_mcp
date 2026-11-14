#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"

bool UEngineWireSubsystem::HandleSequenceAddSpawnable(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ClassName;
  LocalPayload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("className required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath;
  UObject *SeqObj = McpSequence::LoadAssetOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_add_spawnable_from_class"), SeqPath);
  if (!SeqObj) {
    return true;
  }

  UClass *ResolvedClass = ResolveClassByName(ClassName);
  if (!ResolvedClass) {
    SendAutomationError(Socket, RequestId, TEXT("Class not found"),
                        TEXT("CLASS_NOT_FOUND"));
    return true;
  }
  // The spawner makes the template with NewObject: for an abstract class an engine ensure, nulled out on save.
  if (ResolvedClass->HasAnyClassFlags(CLASS_Abstract)) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Class %s is abstract and cannot be spawned; name one of its subclasses."),
                                        *ResolvedClass->GetName()),
                        TEXT("INVALID_CLASS"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    // The sequence's own spawners make a template the sequence owns. AddSpawnable with the class default object
    // stored the CDO itself, which a recompile replaces: "does not have a valid object template", nothing spawned.
    const FGuid BindingGuid = LevelSeq->GetMovieScene()
                                  ? static_cast<UMovieSceneSequence *>(LevelSeq)->CreateSpawnable(ResolvedClass)
                                  : FGuid();
    if (BindingGuid.IsValid()) {
      LevelSeq->MarkPackageDirty();
      TSharedPtr<FJsonObject> SpawnableResp = McpHandlerUtils::CreateResultObject();
      SpawnableResp->SetStringField(TEXT("className"), ResolvedClass->GetPathName());
      SpawnableResp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Spawnable added to sequence"), SpawnableResp, FString());
      return true;
    }
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create spawnable binding"),
                        TEXT("SPAWNABLE_CREATION_FAILED"));
    return true;
  }
  SendAutomationError(Socket, RequestId,
                      TEXT("Sequence object is not a LevelSequence"),
                      TEXT("INVALID_SEQUENCE"));
  return true;
}

bool UEngineWireSubsystem::HandleSequenceGetBindings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  UMovieScene *MovieScene = nullptr;
  if (!McpSequence::LoadOrReply(this, RequestId, Socket, LocalPayload, TEXT("sequence_get_bindings"), MovieScene)) {
    return true;
  }
  TArray<TSharedPtr<FJsonValue>> BindingsArray;
  for (const FMovieSceneBinding &B : const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    TSharedPtr<FJsonObject> Bobj = McpHandlerUtils::CreateResultObject();
    Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());
    Bobj->SetStringField(TEXT("name"), GetBindingName(MovieScene, B.GetObjectGuid()));
    // A component bound under its actor goes by its bare name; its actor's binding tells several apart.
    const FMovieScenePossessable *Possessable = MovieScene->FindPossessable(B.GetObjectGuid());
    if (Possessable && Possessable->GetParent().IsValid()) {
      Bobj->SetStringField(TEXT("parentId"), Possessable->GetParent().ToString());
    }
    BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("bindings"), BindingsArray);
  SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed"), Resp, FString());
  return true;
}
