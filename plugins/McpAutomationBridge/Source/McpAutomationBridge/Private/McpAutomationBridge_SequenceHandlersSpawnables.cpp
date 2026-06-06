#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

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

#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

  UClass *ResolvedClass = nullptr;
  if (ClassName.StartsWith(TEXT("/")) || ClassName.Contains(TEXT("/"))) {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassName)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(ClassName);
  if (!ResolvedClass) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Class not found"),
                           nullptr, TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    UMovieScene *MovieScene = LevelSeq->GetMovieScene();
    if (MovieScene) {
      UObject *DefaultObject = ResolvedClass->GetDefaultObject();
      if (DefaultObject) {
        FGuid BindingGuid = MovieScene->AddSpawnable(ClassName, *DefaultObject);
        if (MovieScene->FindSpawnable(BindingGuid)) {
          MovieScene->Modify();
          TSharedPtr<FJsonObject> SpawnableResp =
              McpHandlerUtils::CreateResultObject();
          SpawnableResp->SetBoolField(TEXT("success"), true);
          SpawnableResp->SetStringField(TEXT("className"), ClassName);
          SpawnableResp->SetStringField(TEXT("bindingGuid"),
                                        BindingGuid.ToString());
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Spawnable added to sequence"),
                                 SpawnableResp, FString());
          return true;
        }
      }
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
#else
  SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_add_spawnable_from_class requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
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
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
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

        FString BindingName;
        if (FMovieScenePossessable *Possessable =
                MovieScene->FindPossessable(B.GetObjectGuid())) {
          BindingName = Possessable->GetName();
        } else if (FMovieSceneSpawnable *Spawnable =
                       MovieScene->FindSpawnable(B.GetObjectGuid())) {
          BindingName = Spawnable->GetName();
        }

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
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_get_bindings requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
