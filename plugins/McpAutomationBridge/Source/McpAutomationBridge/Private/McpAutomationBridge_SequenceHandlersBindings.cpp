#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

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

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (GEditor) {
    UClass *CameraClass = ACameraActor::StaticClass();
    AActor *Spawned = SpawnActorInActiveWorld<AActor>(
        CameraClass, FVector::ZeroVector, FRotator::ZeroRotator,
        TEXT("SequenceCamera"));
    if (Spawned) {
      if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
        if (UMovieScene *MovieScene = LevelSeq->GetMovieScene()) {
          FGuid BindingGuid = MovieScene->AddPossessable(
              Spawned->GetActorLabel(), Spawned->GetClass());
          if (MovieScene->FindPossessable(BindingGuid)) {
            MovieScene->Modify();
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
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("UEditorActorSubsystem not available"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_camera requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
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

#if WITH_EDITOR
  TSharedPtr<FJsonObject> ForwardPayload = McpHandlerUtils::CreateResultObject();
  ForwardPayload->SetStringField(TEXT("path"), SeqPath);
  TArray<TSharedPtr<FJsonValue>> NamesArray;
  NamesArray.Add(MakeShared<FJsonValueString>(ActorName));
  ForwardPayload->SetArrayField(TEXT("actorNames"), NamesArray);
  return HandleSequenceAddActors(RequestId, ForwardPayload, Socket);
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actor requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
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

#if WITH_EDITOR
  TArray<FString> Names;
  Names.Reserve(Arr->Num());
  for (const TSharedPtr<FJsonValue> &V : *Arr) {
    if (V.IsValid() && V->Type == EJson::String)
      Names.Add(V->AsString());
  }

  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  if (!GEditor) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Editor not available"), nullptr,
                                      TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

#if MCP_HAS_EDITOR_ACTOR_SUBSYSTEM
  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Names.Num());
    for (const FString &Name : Names) {
      TSharedPtr<FJsonObject> Item = McpHandlerUtils::CreateResultObject();
      Item->SetStringField(TEXT("name"), Name);
      AActor *Found = Subsystem->FindActorByName(Name);

      if (!Found) {
        Item->SetBoolField(TEXT("success"), false);
        Item->SetStringField(TEXT("error"), TEXT("Actor not found"));
      } else {
        if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
          UMovieScene *MovieScene = LevelSeq->GetMovieScene();
          if (MovieScene) {
            FGuid BindingGuid = MovieScene->AddPossessable(
                Found->GetActorLabel(), Found->GetClass());
            if (MovieScene->FindPossessable(BindingGuid)) {
              Item->SetBoolField(TEXT("success"), true);
              Item->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
              MovieScene->Modify();
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
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Actors processed"), Out, FString());
    return true;
  }
  Subsystem->SendAutomationResponse(
      Socket, RequestIdArg, false, TEXT("EditorActorSubsystem not available"),
      nullptr, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
  return true;
#else
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                    TEXT("UEditorActorSubsystem not available"),
                                    nullptr, TEXT("NOT_AVAILABLE"));
#endif
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_add_actors requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
