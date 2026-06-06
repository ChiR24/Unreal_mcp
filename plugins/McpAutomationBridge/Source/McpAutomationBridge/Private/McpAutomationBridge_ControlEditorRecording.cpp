#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorConsoleCommand(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  return HandleConsoleCommandAction(RequestId, TEXT("console_command"), Payload, Socket);
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Console command requires editor build."), nullptr);
  return true;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlEditorStartRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString RecordingName;
  // Accept both 'name' and 'filename' fields for flexibility
  // TS handler sends 'filename', so we check that first
  Payload->TryGetStringField(TEXT("filename"), RecordingName);
  if (RecordingName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), RecordingName);
  }
  if (RecordingName.IsEmpty()) {
    RecordingName = FString::Printf(TEXT("Recording_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  } else {
    RecordingName = MakeSafeConsoleName(RecordingName, TEXT("Recording"));
  }

  // Use console command to start demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    FString Command = FString::Printf(TEXT("DemoRec %s"), *RecordingName);
    GEditor->Exec(World, *Command);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("message"), TEXT("Recording started"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording started"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStopRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use console command to stop demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("DemoStop"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Recording stopped"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording stopped"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to set bookmark
  FString Command = FString::Printf(TEXT("SetBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Bookmark %d created"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Bookmark created"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to jump to bookmark
  FString Command = FString::Printf(TEXT("JumpToBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Jumped to bookmark %d"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Jumped to bookmark"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}
