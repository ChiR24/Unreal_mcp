#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSpawnable(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString ClassName;
  LocalPayload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("className required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_add_spawnable_from_class requires a sequence path"),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UClass *ResolvedClass = ResolveClassByName(ClassName);
  if (!ResolvedClass) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Class not found"),
                           nullptr, TEXT("CLASS_NOT_FOUND"));
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
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create spawnable binding"), nullptr,
                           TEXT("SPAWNABLE_CREATION_FAILED"));
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Sequence object is not a LevelSequence"),
                         nullptr, TEXT("INVALID_SEQUENCE_TYPE"));
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceGetBindings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_bindings requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
      TArray<TSharedPtr<FJsonValue>> BindingsArray;
      for (const FMovieSceneBinding &B :
           const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
        TSharedPtr<FJsonObject> Bobj = McpHandlerUtils::CreateResultObject();
        Bobj->SetStringField(TEXT("id"), B.GetObjectGuid().ToString());

        FString BindingName = GetBindingName(MovieScene, B.GetObjectGuid());

        Bobj->SetStringField(TEXT("name"), BindingName);
        BindingsArray.Add(MakeShared<FJsonValueObject>(Bobj));
      }
      Resp->SetArrayField(TEXT("bindings"), BindingsArray);
      SendAutomationResponse(Socket, RequestId, true, TEXT("bindings listed"),
                             Resp, FString());
      return true;
    }
  }
  Resp->SetArrayField(TEXT("bindings"), TArray<TSharedPtr<FJsonValue>>());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("bindings listed (empty)"), Resp, FString());
  return true;
}
