#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Modules/ModuleManager.h"

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

  // Drive the requested state instead of blindly toggling (dogfood #142).
  bool bApplied = false;
  FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
  if (TSharedPtr<SLevelViewport> LevelViewport = LevelEditor.GetFirstActiveLevelViewport()) {
    if (LevelViewport->IsImmersive() != bEnabled) {
      LevelViewport->MakeImmersive(bEnabled, false);
    }
    bEnabled = LevelViewport->IsImmersive();
    bApplied = true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  Resp->SetBoolField(TEXT("applied"), bApplied);
  SendAutomationResponse(Socket, RequestId, true, bEnabled ? TEXT("Immersive mode enabled") : TEXT("Immersive mode disabled"), Resp, FString());
  return true;
#else
  return false;
#endif
}
