#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Ui/McpAutomationBridge_UiHandlersPrivate.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
namespace McpUiHandlers {

namespace {

// Friendly category names accepted by get_project_settings {category}.
const TCHAR *SectionForCategory(const FString &Category) {
  static const TMap<FString, const TCHAR *> Map = {
      {TEXT("general"), TEXT("/Script/EngineSettings.GeneralProjectSettings")},
      {TEXT("project"), TEXT("/Script/EngineSettings.GeneralProjectSettings")},
      {TEXT("maps"), TEXT("/Script/EngineSettings.GameMapsSettings")},
      {TEXT("game"), TEXT("/Script/EngineSettings.GameMapsSettings")},
      {TEXT("mapsandmodes"), TEXT("/Script/EngineSettings.GameMapsSettings")},
      {TEXT("rendering"), TEXT("/Script/Engine.RendererSettings")},
      {TEXT("input"), TEXT("/Script/Engine.InputSettings")},
      {TEXT("physics"), TEXT("/Script/Engine.PhysicsSettings")},
      {TEXT("collision"), TEXT("/Script/Engine.CollisionProfile")},
      {TEXT("audio"), TEXT("/Script/Engine.AudioSettings")},
      {TEXT("engine"), TEXT("/Script/Engine.Engine")},
      {TEXT("gameusersettings"), TEXT("/Script/Engine.GameUserSettings")},
      {TEXT("navigation"), TEXT("/Script/NavigationSystem.NavigationSystemV1")},
      {TEXT("streaming"), TEXT("/Script/Engine.StreamingSettings")},
      {TEXT("garbagecollection"), TEXT("/Script/Engine.GarbageCollectionSettings")},
  };
  FString Key = Category.ToLower();
  Key.ReplaceInline(TEXT(" "), TEXT(""));
  Key.ReplaceInline(TEXT("_"), TEXT(""));
  const TCHAR *const *Found = Map.Find(Key);
  return Found ? *Found : nullptr;
}

// "/Script/Module.Class" -> UClass, tolerating a bare "Module.Class".
UClass *ResolveSettingsClass(const FString &Section) {
  FString Path = Section.TrimStartAndEnd();
  if (Path.IsEmpty()) {
    return nullptr;
  }
  if (!Path.StartsWith(TEXT("/"))) {
    Path = TEXT("/Script/") + Path;
  }
  UClass *Class = FindObject<UClass>(nullptr, *Path);
  if (!Class) {
    Class = LoadObject<UClass>(nullptr, *Path);
  }
  return Class;
}

// Every config-flagged property of the class default object, as text.
TSharedPtr<FJsonObject> ExportConfigProperties(UClass *Class, int32 &OutCount) {
  TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
  OutCount = 0;
  UObject *CDO = Class ? Class->GetDefaultObject() : nullptr;
  if (!CDO) {
    return Values;
  }
  for (TFieldIterator<FProperty> It(Class); It; ++It) {
    FProperty *Property = *It;
    if (!Property || !Property->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig)) {
      continue;
    }
    FString Text;
    Property->ExportText_InContainer(0, Text, CDO, CDO, CDO, PPF_None);
    Values->SetStringField(Property->GetName(), Text);
    ++OutCount;
  }
  return Values;
}

} // namespace

bool HandleProjectSettingsAction(const FString &LowerSub,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 bool &bSuccess, FString &Message,
                                 FString &ErrorCode) {
  if (LowerSub == TEXT("get_project_settings")) {
    FString Section;
    Payload->TryGetStringField(TEXT("section"), Section);
    FString Category;
    Payload->TryGetStringField(TEXT("category"), Category);
    if (Section.IsEmpty() && !Category.IsEmpty()) {
      const TCHAR *Mapped = SectionForCategory(Category);
      if (!Mapped) {
        Message = FString::Printf(
            TEXT("Unknown settings category '%s'. Use a section path such as /Script/Engine.RendererSettings, or one of: general, maps, rendering, input, physics, collision, audio, engine, navigation"),
            *Category);
        ErrorCode = TEXT("NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
      Section = Mapped;
    }

    if (!Section.IsEmpty()) {
      UClass *Class = ResolveSettingsClass(Section);
      if (!Class) {
        Message = FString::Printf(TEXT("Settings class not found for section '%s'"), *Section);
        ErrorCode = TEXT("NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
      int32 Count = 0;
      TSharedPtr<FJsonObject> Values = ExportConfigProperties(Class, Count);
      FString Key;
      Payload->TryGetStringField(TEXT("key"), Key);
      if (!Key.IsEmpty()) {
        FString Value;
        if (!Values->TryGetStringField(Key, Value)) {
          // UProperty lookup missed: the value may still exist as a raw INI
          // entry (console variables like r.AllowStaticLighting, or keys only
          // meaningful to INI readers). Fall back to the class config file so
          // a set→get round-trip through set_project_setting reads back instead
          // of reporting NOT_FOUND for a value that is genuinely on disk.
          // NOTE: the filename must be the FULL project config path — a bare
          // "DefaultEngine.ini" can resolve to the engine install's file
          // instead of the project's.
          const FString ConfigFile = FPaths::ProjectConfigDir() / FString::Printf(
              TEXT("Default%s.ini"), *Class->ClassConfigName.ToString());
          if (GConfig->GetString(*Section, *Key, Value, *ConfigFile)) {
            Resp->SetStringField(TEXT("key"), Key);
            Resp->SetStringField(TEXT("value"), Value);
            Resp->SetStringField(TEXT("source"), TEXT("ini"));
            Resp->SetStringField(
                TEXT("note"),
                TEXT("Raw INI entry, not a live UProperty: present on disk but not applied to the running editor."));
          } else {
            Message = FString::Printf(TEXT("Setting '%s' not found in %s"), *Key, *Class->GetPathName());
            ErrorCode = TEXT("NOT_FOUND");
            Resp->SetStringField(TEXT("error"), Message);
            return true;
          }
        } else {
          Resp->SetStringField(TEXT("key"), Key);
          Resp->SetStringField(TEXT("value"), Value);
        }
      }
      Resp->SetStringField(TEXT("section"), Class->GetPathName());
      Resp->SetStringField(TEXT("configName"), Class->ClassConfigName.ToString());
      Resp->SetObjectField(TEXT("settings"), Values);
      Resp->SetNumberField(TEXT("settingCount"), Count);
      bSuccess = true;
      Message = FString::Printf(TEXT("Project settings retrieved (%d config properties)"), Count);
      return true;
    }

    // No section: a compact project overview.
    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
    SettingsObj->SetStringField(
        TEXT("engineVersion"),
        FString::Printf(TEXT("%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION));
    SettingsObj->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    SettingsObj->SetStringField(TEXT("projectDir"), FPaths::ProjectDir());
    if (UClass *MapsClass = ResolveSettingsClass(TEXT("/Script/EngineSettings.GameMapsSettings"))) {
      int32 Count = 0;
      SettingsObj->SetObjectField(TEXT("maps"), ExportConfigProperties(MapsClass, Count));
    }
    if (UClass *GeneralClass = ResolveSettingsClass(TEXT("/Script/EngineSettings.GeneralProjectSettings"))) {
      int32 Count = 0;
      SettingsObj->SetObjectField(TEXT("general"), ExportConfigProperties(GeneralClass, Count));
    }
    Resp->SetObjectField(TEXT("settings"), SettingsObj);
    Resp->SetStringField(TEXT("hint"), TEXT("Pass section (e.g. /Script/Engine.RendererSettings) or category (rendering, input, physics, ...) for a full section dump."));
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

  // The plugin's own settings live under this section; letting the automation
  // channel rewrite them could disable its own token authentication, so that
  // section is refused outright.
  if (NormalizedSection.Contains(TEXT("McpAutomationBridge"))) {
    Message = TEXT("set_project_setting cannot target the McpAutomationBridge settings section");
    ErrorCode = TEXT("SETTING_NOT_PERMITTED");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  // Apply to the live settings object when the section is a config class, so
  // the editor sees the change immediately, then persist it to the project's
  // Default<Config>.ini (the old code only wrote DefaultEngine.ini in memory).
  UClass *Class = ResolveSettingsClass(NormalizedSection);
  FString ConfigFile = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
  bool bAppliedToObject = false;
  bool bPersisted = false;
  if (Class) {
    ConfigFile = FPaths::ProjectConfigDir() / FString::Printf(TEXT("Default%s.ini"), *Class->ClassConfigName.ToString());
    if (UObject *CDO = Class->GetDefaultObject()) {
      FProperty *Property = Class->FindPropertyByName(FName(*Key));
      if (!Property) {
        // Renderer and engine settings publish their INI key through
        // ConsoleVariable metadata rather than the property name: the key
        // "r.GPUSkin.Support16BitBoneIndex" belongs to bSupport16BitBoneIndex.
        // Matching on the name alone missed that whole family -- most of the
        // r.* surface -- so those keys fell through to the GConfig path, which
        // cannot write a Default*.ini at all.
        for (TFieldIterator<FProperty> It(Class); It; ++It) {
          if (It->GetMetaData(TEXT("ConsoleVariable")) == Key) {
            Property = *It;
            break;
          }
        }
      }
      if (Property) {
        void *ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
        if (Property->ImportText_Direct(*Value, ValuePtr, CDO, PPF_None)) {
          bAppliedToObject = true;
          bPersisted = CDO->TryUpdateDefaultConfigFile(FString(), false);
        } else {
          Message = FString::Printf(TEXT("Value '%s' could not be parsed for %s.%s (%s)"), *Value, *Class->GetName(), *Key, *Property->GetCPPType());
          ErrorCode = TEXT("INVALID_VALUE");
          Resp->SetStringField(TEXT("error"), Message);
          return true;
        }
      }
    }
  }
  // GConfig keys its file map by the EXACT path string it is handed, and
  // FPaths::ProjectConfigDir() is relative ("../../../../../Games/X/Config/").
  // That does not match the absolute name the engine loaded the file under, so
  // SetString lands on a phantom entry and Flush writes nothing -- the engine
  // says so with "GConfig::Find attempting to access config with
  // non-normalized path". Callers were still told persisted:true, so a setting
  // that never reached disk was reported as written.
  ConfigFile = FPaths::ConvertRelativePathToFull(ConfigFile);

  if (!bPersisted) {
    GConfig->SetString(*NormalizedSection, *Key, *Value, ConfigFile);
    GConfig->Flush(false, ConfigFile);
    // Evidence, not assumption: read the file back. A flush that silently
    // no-ops must not be reported as a successful write.
    FString OnDisk;
    const FString Assignment = Key + TEXT("=");
    bPersisted = FFileHelper::LoadFileToString(OnDisk, *ConfigFile) && OnDisk.Contains(Assignment);
    if (!bPersisted) {
      Message = FString::Printf(
          TEXT("%s.%s was not written to %s: the config flush left no '%s' on disk."),
          *NormalizedSection, *Key, *ConfigFile, *Assignment);
      ErrorCode = TEXT("PERSIST_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
  }

  // Console-variable-backed keys (r.* and the many engine settings that mirror a
  // CVar) used to stop at the INI with a "restart" caveat. When a registered,
  // writable CVar matches the key, apply it live too so the running editor
  // reflects the setting immediately. Read-only CVars are left alone: the engine
  // warns on a write to them and they are restart-only by design.
  bool bAppliedToCvar = false;
  if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(*Key)) {
    if (!CVar->TestFlags(ECVF_ReadOnly)) {
      CVar->Set(*Value, ECVF_SetByProjectSetting);
      bAppliedToCvar = true;
    }
  }

  Resp->SetStringField(TEXT("section"), NormalizedSection);
  Resp->SetStringField(TEXT("key"), Key);
  Resp->SetStringField(TEXT("value"), Value);
  Resp->SetStringField(TEXT("configFile"), ConfigFile);
  Resp->SetBoolField(TEXT("appliedToLiveSettings"), bAppliedToObject);
  Resp->SetBoolField(TEXT("appliedToConsoleVariable"), bAppliedToCvar);
  Resp->SetBoolField(TEXT("persisted"), bPersisted);
  bSuccess = true;
  if (bAppliedToObject) {
    Message = FString::Printf(TEXT("Set %s.%s = %s"), *NormalizedSection, *Key, *Value);
  } else if (bAppliedToCvar) {
    Message = FString::Printf(
        TEXT("Set %s.%s = %s — applied live to the matching console variable and persisted to %s."),
        *NormalizedSection, *Key, *Value, *ConfigFile);
  } else {
    // Config-file-only write: no UProperty matched (e.g. a console variable
    // such as r.AllowStaticLighting, or a key on an unknown section), so the
    // running editor is unchanged. Previously the message claimed "Set ..."
    // unconditionally, which read as a live change that never happened.
    Message = FString::Printf(
        TEXT("Wrote %s.%s = %s to %s (config file only). No live UProperty or registered console variable matched, so the running editor is unchanged; INI readers pick it up on restart. For an immediate effect, target a UProperty- or CVar-backed key."),
        *NormalizedSection, *Key, *Value, *ConfigFile);
    Resp->SetStringField(TEXT("warning"), Message);
  }
  return true;
}

} // namespace McpUiHandlers
#endif
