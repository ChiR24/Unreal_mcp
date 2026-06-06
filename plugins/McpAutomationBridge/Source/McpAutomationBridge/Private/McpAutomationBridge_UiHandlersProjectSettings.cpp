#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_UiHandlersPrivate.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
namespace McpUiHandlers {

bool HandleProjectSettingsAction(const FString &LowerSub,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 bool &bSuccess, FString &Message,
                                 FString &ErrorCode) {
  if (LowerSub == TEXT("get_project_settings")) {
    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();

    if (GEngine) {
      SettingsObj->SetStringField(
          TEXT("engineVersion"),
          FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION,
                          ENGINE_MINOR_VERSION));
      SettingsObj->SetStringField(TEXT("projectName"), FApp::GetProjectName());
      SettingsObj->SetStringField(TEXT("projectDir"), FPaths::ProjectDir());

      FString ResolutionX;
      FString ResolutionY;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"),
                         TEXT("ResolutionSizeX"), ResolutionX,
                         GGameUserSettingsIni);
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"),
                         TEXT("ResolutionSizeY"), ResolutionY,
                         GGameUserSettingsIni);
      if (!ResolutionX.IsEmpty() && !ResolutionY.IsEmpty()) {
        TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
        ResObj->SetStringField(TEXT("width"), ResolutionX);
        ResObj->SetStringField(TEXT("height"), ResolutionY);
        SettingsObj->SetObjectField(TEXT("resolution"), ResObj);
      }

      FString FullscreenMode;
      GConfig->GetString(TEXT("/Script/Engine.GameUserSettings"),
                         TEXT("LastConfirmedFullscreenMode"), FullscreenMode,
                         GGameUserSettingsIni);
      if (!FullscreenMode.IsEmpty()) {
        SettingsObj->SetStringField(TEXT("fullscreenMode"), FullscreenMode);
      }
    }

    Resp->SetObjectField(TEXT("settings"), SettingsObj);
    bSuccess = true;
    Message = TEXT("Project settings retrieved");
    return true;
  }

  if (LowerSub != TEXT("set_project_setting")) {
    return false;
  }

  FString Section;
  FString Key;
  FString Value;
  Payload->TryGetStringField(TEXT("section"), Section);
  Payload->TryGetStringField(TEXT("key"), Key);
  Payload->TryGetStringField(TEXT("value"), Value);

  if (Section.IsEmpty() || Key.IsEmpty()) {
    Message = TEXT("section and key are required for set_project_setting");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  FString NormalizedSection = Section;
  if (!NormalizedSection.StartsWith(TEXT("/")) &&
      !NormalizedSection.StartsWith(TEXT("["))) {
    NormalizedSection = FString::Printf(TEXT("/Script/%s"), *Section);
  }

  const FString ConfigFile =
      FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
  GConfig->SetString(*NormalizedSection, *Key, *Value, ConfigFile);
  GConfig->Flush(false, ConfigFile);

  Resp->SetStringField(TEXT("section"), NormalizedSection);
  Resp->SetStringField(TEXT("key"), Key);
  Resp->SetStringField(TEXT("value"), Value);
  bSuccess = true;
  Message = FString::Printf(TEXT("Set %s.%s = %s"), *NormalizedSection, *Key,
                            *Value);
  return true;
}

}
#endif
