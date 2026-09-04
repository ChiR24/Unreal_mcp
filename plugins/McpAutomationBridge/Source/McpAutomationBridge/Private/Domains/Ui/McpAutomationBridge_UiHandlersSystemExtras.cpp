#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Ui/McpAutomationBridge_UiHandlersPrivate.h"
#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#if WITH_EDITOR
namespace McpUiHandlers {

namespace {

FString ReadFirstString(const TSharedPtr<FJsonObject> &Payload,
                        std::initializer_list<const TCHAR *> Keys) {
  for (const TCHAR *Key : Keys) {
    FString Value;
    if (Payload->TryGetStringField(Key, Value) && !Value.TrimStartAndEnd().IsEmpty()) {
      return Value.TrimStartAndEnd();
    }
  }
  return FString();
}

// "1920x1080" or width/height fields -> "1920x1080"; empty when absent.
FString ReadResolution(const TSharedPtr<FJsonObject> &Payload) {
  FString Resolution = ReadFirstString(Payload, {TEXT("resolution")});
  if (!Resolution.IsEmpty()) {
    return Resolution;
  }
  double Width = 0.0;
  double Height = 0.0;
  if (Payload->TryGetNumberField(TEXT("width"), Width) &&
      Payload->TryGetNumberField(TEXT("height"), Height) && Width > 0 && Height > 0) {
    return FString::Printf(TEXT("%dx%d"), static_cast<int32>(Width), static_cast<int32>(Height));
  }
  return FString();
}

bool RunConsole(UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
                const FString &Command, TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Every derived command goes through the validated console_command path so
  // the CommandValidator policy applies exactly as for a direct call.
  TSharedPtr<FJsonObject> CommandPayload = McpHandlerUtils::CreateResultObject();
  CommandPayload->SetStringField(TEXT("command"), Command);
  return FMcpUiHandlerAccess::ConsoleCommand(Bridge, RequestId, CommandPayload, Socket);
}

} // namespace

// The eight system_control actions that had describable contracts but no
// native implementation (dogfood #167): each is a thin adapter onto an
// existing handler or a validated console command.
bool HandleSystemExtrasAction(UMcpAutomationBridgeSubsystem &Bridge,
                              const FString &RequestId, const FString &LowerSub,
                              const TSharedPtr<FJsonObject> &Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (LowerSub == TEXT("lumen_update_scene")) {
    return McpRenderHandlers::HandleLumenUpdateScene(&Bridge, RequestId, Socket);
  }
  if (LowerSub == TEXT("spawn_category")) {
    return FMcpUiHandlerAccess::Debug(Bridge, RequestId, TEXT("spawn_category"), Payload, Socket);
  }
  if (LowerSub == TEXT("play_sound")) {
    TSharedPtr<FJsonObject> AudioPayload = McpHandlerUtils::CreateResultObject();
    AudioPayload->Values = Payload->Values;
    const FString SoundPath = ReadFirstString(Payload, {TEXT("soundPath"), TEXT("assetPath"), TEXT("path")});
    if (SoundPath.IsEmpty()) {
      Bridge.SendAutomationError(Socket, RequestId, TEXT("soundPath is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    AudioPayload->SetStringField(TEXT("soundPath"), SoundPath);
    AudioPayload->SetStringField(TEXT("assetPath"), SoundPath);
    return FMcpUiHandlerAccess::Audio(Bridge, RequestId, TEXT("play_sound_2d"), AudioPayload, Socket);
  }
  if (LowerSub == TEXT("set_cvar")) {
    const FString Name = ReadFirstString(Payload, {TEXT("name"), TEXT("cvar"), TEXT("key"), TEXT("command")});
    FString Value;
    if (const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"))) {
      if (ValueField->Type == EJson::Boolean) {
        Value = ValueField->AsBool() ? TEXT("1") : TEXT("0");
      } else if (ValueField->Type == EJson::Number) {
        Value = FString::SanitizeFloat(ValueField->AsNumber());
      } else {
        Value = ValueField->AsString();
      }
    }
    if (Name.IsEmpty()) {
      Bridge.SendAutomationError(Socket, RequestId, TEXT("name (or cvar/key/command) is required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    return RunConsole(Bridge, RequestId, Value.IsEmpty() ? Name : Name + TEXT(" ") + Value, Socket);
  }
  if (LowerSub == TEXT("set_resolution")) {
    const FString Resolution = ReadResolution(Payload);
    if (Resolution.IsEmpty()) {
      Bridge.SendAutomationError(Socket, RequestId, TEXT("resolution (WxH) or width and height are required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool bWindowed = true;
    Payload->TryGetBoolField(TEXT("windowed"), bWindowed);
    return RunConsole(Bridge, RequestId, FString::Printf(TEXT("r.SetRes %s%s"), *Resolution, bWindowed ? TEXT("w") : TEXT("f")), Socket);
  }
  if (LowerSub == TEXT("set_fullscreen")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    const FString Resolution = ReadResolution(Payload);
    if (!Resolution.IsEmpty()) {
      return RunConsole(Bridge, RequestId, FString::Printf(TEXT("r.SetRes %s%s"), *Resolution, bEnabled ? TEXT("f") : TEXT("w")), Socket);
    }
    return RunConsole(Bridge, RequestId, FString::Printf(TEXT("r.FullScreenMode %d"), bEnabled ? 0 : 2), Socket);
  }
  if (LowerSub == TEXT("profile")) {
    static const TMap<FString, FString> StatByType = {
        {TEXT("cpu"), TEXT("stat unit")},          {TEXT("gpu"), TEXT("stat gpu")},
        {TEXT("memory"), TEXT("stat memory")},     {TEXT("renderthread"), TEXT("stat scenerendering")},
        {TEXT("gamethread"), TEXT("stat game")},   {TEXT("all"), TEXT("stat unitgraph")}};
    FString ProfileType = ReadFirstString(Payload, {TEXT("profileType"), TEXT("type"), TEXT("statName"), TEXT("stat")}).ToLower();
    ProfileType.ReplaceInline(TEXT(" "), TEXT(""));
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    if (!bEnabled) {
      return RunConsole(Bridge, RequestId, TEXT("stat none"), Socket);
    }
    const FString *Command = StatByType.Find(ProfileType.IsEmpty() ? TEXT("cpu") : ProfileType);
    if (!Command) {
      Bridge.SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Unknown profileType '%s' (CPU, GPU, Memory, RenderThread, GameThread, All)"), *ProfileType),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    return RunConsole(Bridge, RequestId, *Command, Socket);
  }
  if (LowerSub == TEXT("show_widget")) {
    const FString WidgetId = ReadFirstString(Payload, {TEXT("widgetId"), TEXT("widget"), TEXT("name")});
    const FString Message = ReadFirstString(Payload, {TEXT("message"), TEXT("text")});
    if (!WidgetId.IsEmpty() && !WidgetId.Equals(TEXT("notification"), ESearchCase::IgnoreCase)) {
      Bridge.SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Unknown widgetId '%s'; only 'notification' is supported"), *WidgetId),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FNotificationInfo Info(FText::FromString(Message.IsEmpty() ? TEXT("MCP notification") : Message));
    Info.ExpireDuration = 4.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("widgetId"), TEXT("notification"));
    Result->SetStringField(TEXT("message"), Message);
    Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Notification shown"), Result);
    return true;
  }
  return false;
}

} // namespace McpUiHandlers
#endif
