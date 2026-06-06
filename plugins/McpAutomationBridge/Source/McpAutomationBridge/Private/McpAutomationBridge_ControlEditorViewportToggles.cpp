#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  TArray<FString> StatsShown;
  if (World) {
    GEditor->Exec(World, TEXT("Stat FPS"));
    StatsShown.Add(TEXT("FPS"));
    GEditor->Exec(World, TEXT("Stat Unit"));
    StatsShown.Add(TEXT("Unit"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> StatsArray;
  for (const FString& Stat : StatsShown) {
    StatsArray.Add(MakeShared<FJsonValueString>(Stat));
  }
  Resp->SetArrayField(TEXT("statsShown"), StatsArray);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats displayed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorHideStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("Stat None"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats hidden"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Toggle game view via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(),
                bEnabled ? TEXT("ToggleGameView 1") : TEXT("ToggleGameView 0"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("gameViewEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Game view %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetImmersiveMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  // Toggle immersive mode - this is viewport-specific
  if (GEditor && GEditor->GetActiveViewport()) {
    FViewport* Viewport = GEditor->GetActiveViewport();
    if (Viewport) {
      // Immersive mode toggle via console
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("ToggleImmersive"));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Immersive mode toggled"), Resp, FString());
  return true;
#else
  return false;
#endif
}
