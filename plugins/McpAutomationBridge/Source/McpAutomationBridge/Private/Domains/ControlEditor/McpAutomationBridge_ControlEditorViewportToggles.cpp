#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "Modules/ModuleManager.h"

namespace {
// Whether a stat is on screen in the viewport a Stat command lands on
// (UnrealEdSrv.cpp): the running game's, else the level viewport used last.
// Stat commands toggle, so this is what keeps show_stats from hiding a stat that
// is already up (and hide_stats from showing one that is not).
bool McpIsStatShown(const FString &Stat) {
  if (GEditor->GameViewport && !GEditor->GameViewport->IsSimulateInEditorViewport()) {
    return GEditor->GameViewport->IsStatEnabled(Stat);
  }
  const FLevelEditorViewportClient *Client =
      GLastKeyLevelEditingViewportClient ? GLastKeyLevelEditingViewportClient : GCurrentLevelEditingViewportClient;
  return Client && Client->IsRealtime() && Client->ShouldShowStats() && Client->IsStatEnabled(Stat);
}

bool McpHasStatViewport() {
  return (GEditor->GameViewport && !GEditor->GameViewport->IsSimulateInEditorViewport()) ||
         GLastKeyLevelEditingViewportClient || GCurrentLevelEditingViewportClient;
}

// The requested stat (one console token: letters, digits, underscores), or
// Defaults when none was given. False with OutError on a bad name.
bool McpReadStatNames(const TSharedPtr<FJsonObject> &Payload, const TArray<FString> &Defaults,
                      TArray<FString> &Out, FString &OutError) {
  FString Stat;
  Payload->TryGetStringField(TEXT("stat"), Stat);
  Stat.TrimStartAndEndInline();
  if (Stat.IsEmpty()) {
    Out = Defaults;
    return true;
  }
  for (const TCHAR Ch : Stat) {
    if (!FChar::IsAlnum(Ch) && Ch != TEXT('_')) {
      OutError = FString::Printf(TEXT("stat must be one stat name such as FPS, Unit or Game, got '%s'"), *Stat);
      return false;
    }
  }
  Out.Add(Stat);
  return true;
}

TArray<TSharedPtr<FJsonValue>> McpStatsJson(const TArray<FString> &Names) {
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FString &Name : Names)
    Out.Add(MakeShared<FJsonValueString>(Name));
  return Out;
}

// Shows (bShow) or hides each stat, toggling only the ones not already in the
// wanted state, and sends the reply. hide_stats with no stat hides them all.
void McpSetStatsShown(UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
                      const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                      bool bShow) {
  TArray<FString> Stats;
  FString Error;
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!World || !McpHasStatViewport()) {
    Bridge.SendAutomationError(Socket, RequestId, TEXT("No viewport is open to show stats in"), TEXT("NO_VIEWPORT"));
    return;
  }
  const TArray<FString> Defaults = bShow ? TArray<FString>{TEXT("FPS"), TEXT("Unit")} : TArray<FString>();
  if (!McpReadStatNames(Payload, Defaults, Stats, Error)) {
    Bridge.SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
    return;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Stats.Num() == 0) {
    GEditor->Exec(World, TEXT("Stat None"));
    Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
    Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("All stats hidden"), Resp);
    return;
  }
  TArray<FString> Toggled, Unchanged;
  for (const FString &Stat : Stats) {
    if (McpIsStatShown(Stat) == bShow) {
      Unchanged.Add(Stat);
      continue;
    }
    const FString Command = FString(TEXT("Stat ")) + Stat;
    GEditor->Exec(World, *Command);
    Toggled.Add(Stat);
  }
  Resp->SetArrayField(bShow ? TEXT("statsShown") : TEXT("statsHidden"), McpStatsJson(Toggled));
  Resp->SetArrayField(bShow ? TEXT("alreadyShown") : TEXT("alreadyHidden"), McpStatsJson(Unchanged));
  Bridge.SendAutomationResponse(Socket, RequestId, true,
                                FString::Printf(TEXT("%s: %s"), bShow ? TEXT("Stats shown") : TEXT("Stats hidden"),
                                                *FString::Join(Stats, TEXT(", "))),
                                Resp);
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleControlEditorShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  McpSetStatsShown(*this, RequestId, Payload, Socket, true);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorHideStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  McpSetStatsShown(*this, RequestId, Payload, Socket, false);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetImmersiveMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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

  // With no level viewport nothing changed; that used to report success.
  if (!bApplied) {
    SendAutomationError(Socket, RequestId, TEXT("No active level viewport to make immersive"), TEXT("NO_VIEWPORT"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  Resp->SetBoolField(TEXT("applied"), bApplied);
  SendAutomationResponse(Socket, RequestId, true, bEnabled ? TEXT("Immersive mode enabled") : TEXT("Immersive mode disabled"), Resp, FString());
  return true;
}
