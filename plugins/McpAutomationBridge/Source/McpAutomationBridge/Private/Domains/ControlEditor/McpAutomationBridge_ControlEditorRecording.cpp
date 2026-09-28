#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Bookmarks/IBookmarkTypeTools.h"
#include "Engine/DemoNetDriver.h"
#include "HAL/IConsoleManager.h"
#include "LevelEditorViewport.h"
#include "TimerManager.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorConsoleCommand(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  return HandleConsoleCommandAction(RequestId, TEXT("console_command"), Payload, Socket);
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStartRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString RecordingName;
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

  double FrameRate = 0.0, DurationSeconds = 0.0;
  const bool bHasFrameRate = Payload->TryGetNumberField(TEXT("frameRate"), FrameRate);
  const bool bHasDuration = Payload->TryGetNumberField(TEXT("durationSeconds"), DurationSeconds);
  if ((bHasFrameRate && (FrameRate < 1.0 || FrameRate > 120.0)) || (bHasDuration && DurationSeconds <= 0.0)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("frameRate must be 1 to 120 and durationSeconds above 0"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // A replay records the running game: the editor world has no game instance,
  // so DemoRec there did nothing while this still said "Recording started".
  UWorld* World = GEditor->PlayWorld.Get();
  if (!World) {
    SendAutomationError(Socket, RequestId,
                        TEXT("A replay records a running game; start Play In Editor first (control_editor play)"),
                        TEXT("NO_ACTIVE_SESSION"));
    return true;
  }
  if (bHasFrameRate) {
    if (IConsoleVariable* RecordHz = IConsoleManager::Get().FindConsoleVariable(TEXT("demo.RecordHz"))) {
      RecordHz->Set(static_cast<float>(FrameRate), ECVF_SetByCode);
    }
  }
  GEditor->Exec(World, *FString::Printf(TEXT("DemoRec %s"), *RecordingName));
  if (!World->GetDemoNetDriver()) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("DemoRec %s did not start a replay recorder in the running game"), *RecordingName),
                        TEXT("RECORDING_NOT_STARTED"));
    return true;
  }
  // Stop on its own after durationSeconds of game time.
  if (bHasDuration) {
    FTimerHandle StopHandle;
    World->GetTimerManager().SetTimer(
        StopHandle,
        FTimerDelegate::CreateWeakLambda(World, [World]() {
          if (GEditor) {
            GEditor->Exec(World, TEXT("DemoStop"));
          }
        }),
        static_cast<float>(DurationSeconds), false);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("filename"), RecordingName);
  if (bHasFrameRate) {
    Resp->SetNumberField(TEXT("frameRate"), FrameRate);
  }
  if (bHasDuration) {
    Resp->SetNumberField(TEXT("durationSeconds"), DurationSeconds);
  }
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Recording started: %s"), *RecordingName), Resp, FString());
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStopRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

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
}

namespace {
// The level viewport bookmarks belong to, and the slot named by id (the declared
// name; index is its older spelling). A non-numeric id used to read as 0 and
// silently overwrite slot 0; now it is an error, as is a slot the level cannot hold.
FEditorViewportClient* McpReadBookmarkSlot(const TSharedPtr<FJsonObject>& Payload, uint32& OutSlot, FString& OutError) {
  FEditorViewportClient* Client = GCurrentLevelEditingViewportClient;
  if (!Client) {
    OutError = TEXT("No level viewport is open to hold bookmarks");
    return nullptr;
  }
  TSharedPtr<FJsonValue> Field = Payload->TryGetField(TEXT("id"));
  if (!Field.IsValid()) {
    Field = Payload->TryGetField(TEXT("index"));
  }
  double Number = -1.0;
  if (Field.IsValid() && Field->Type == EJson::String && FCString::IsNumeric(*Field->AsString())) {
    Number = FCString::Atod(*Field->AsString());
  } else if (Field.IsValid() && Field->Type == EJson::Number) {
    Number = Field->AsNumber();
  }
  const uint32 Max = IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(Client);
  if (Number < 0.0 || Number >= Max || Number != FMath::FloorToDouble(Number)) {
    OutError = FString::Printf(TEXT("id must be a bookmark slot number from 0 to %u"), Max > 0 ? Max - 1 : 0);
    return nullptr;
  }
  OutSlot = static_cast<uint32>(Number);
  return Client;
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // There is no SetBookmark console command: the Exec this used to send did
  // nothing, and "Bookmark created" came back anyway.
  uint32 Slot = 0;
  FString Error;
  FEditorViewportClient* Client = McpReadBookmarkSlot(Payload, Slot, Error);
  if (!Client) {
    SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  IBookmarkTypeTools::Get().CreateOrSetBookmark(Slot, Client);
  if (!IBookmarkTypeTools::Get().CheckBookmark(Slot, Client)) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Bookmark %u was not stored"), Slot),
                        TEXT("BOOKMARK_NOT_SET"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("index"), Slot);
  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Bookmark %u created"), Slot), Resp);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  uint32 Slot = 0;
  FString Error;
  FEditorViewportClient* Client = McpReadBookmarkSlot(Payload, Slot, Error);
  if (!Client) {
    SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!IBookmarkTypeTools::Get().CheckBookmark(Slot, Client)) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Bookmark %u is not set; create it first"), Slot),
                        TEXT("BOOKMARK_NOT_FOUND"));
    return true;
  }
  IBookmarkTypeTools::Get().JumpToBookmark(Slot, TSharedPtr<struct FBookmarkBaseJumpToSettings>(), Client);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetNumberField(TEXT("index"), Slot);
  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Jumped to bookmark %u"), Slot), Resp);
  return true;
}
