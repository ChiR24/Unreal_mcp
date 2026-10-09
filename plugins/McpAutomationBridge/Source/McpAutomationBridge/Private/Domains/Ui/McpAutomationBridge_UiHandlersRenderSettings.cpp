#include "Domains/Ui/McpAutomationBridge_UiHandlersPrivate.h"

#include "Engine/RendererSettings.h"
#include "HAL/IConsoleManager.h"

// configure_rendering: the project's rendering methods by plain name (global illumination, reflections, shadows,
// anti-aliasing, hardware ray tracing, distance fields, MegaLights, and the default bloom, auto exposure and motion
// blur). Each goes to the renderer settings and DefaultEngine.ini, and to its console variable when that can change
// live; the reply reads every one back and names those that wait for an editor restart. set_project_setting took raw
// property names and enum spellings and never said a change needed a restart.
namespace McpUiHandlers {
namespace {
struct FMcpRenderSetting {
  const TCHAR *Param;
  const TCHAR *Property;
  // An enum setting's plain values and the enum entries they stand for; empty for a true/false setting.
  TArray<TPair<FString, FString>> Values;
};

const TArray<FMcpRenderSetting> &McpRenderSettings() {
  static const TArray<FMcpRenderSetting> Settings = {
      {TEXT("globalIllumination"), TEXT("DynamicGlobalIllumination"), {{TEXT("lumen"), TEXT("Lumen")}, {TEXT("screen_space"), TEXT("ScreenSpace")}, {TEXT("none"), TEXT("None")}}},
      {TEXT("reflections"), TEXT("Reflections"), {{TEXT("lumen"), TEXT("Lumen")}, {TEXT("screen_space"), TEXT("ScreenSpace")}, {TEXT("none"), TEXT("None")}}},
      {TEXT("shadows"), TEXT("ShadowMapMethod"), {{TEXT("virtual"), TEXT("VirtualShadowMaps")}, {TEXT("maps"), TEXT("ShadowMaps")}}},
      {TEXT("antiAliasing"), TEXT("DefaultFeatureAntiAliasing"),
       {{TEXT("tsr"), TEXT("AAM_TSR")}, {TEXT("taa"), TEXT("AAM_TemporalAA")}, {TEXT("fxaa"), TEXT("AAM_FXAA")}, {TEXT("msaa"), TEXT("AAM_MSAA")}, {TEXT("none"), TEXT("AAM_None")}}},
      {TEXT("hardwareRayTracing"), TEXT("bEnableRayTracing"), {}},
      {TEXT("meshDistanceFields"), TEXT("bGenerateMeshDistanceFields"), {}},
      {TEXT("megaLights"), TEXT("bEnableMegaLights"), {}},
      {TEXT("bloom"), TEXT("bDefaultFeatureBloom"), {}},
      {TEXT("autoExposure"), TEXT("bDefaultFeatureAutoExposure"), {}},
      {TEXT("motionBlur"), TEXT("bDefaultFeatureMotionBlur"), {}},
  };
  return Settings;
}

FString McpPlainKey(const FString &Text) {
  return Text.Replace(TEXT("_"), TEXT("")).Replace(TEXT(" "), TEXT("")).ToLower();
}

UEnum *McpSettingEnum(const FProperty *Property) {
  if (const FByteProperty *Byte = CastField<FByteProperty>(Property)) {
    return Byte->Enum;
  }
  const FEnumProperty *EnumProperty = CastField<FEnumProperty>(Property);
  return EnumProperty ? EnumProperty->GetEnum() : nullptr;
}

// The setting's value as a number: an enum entry's value, or 1 and 0 for true and false.
int64 McpReadSetting(const FProperty *Property, const URendererSettings *Settings) {
  if (const FBoolProperty *Bool = CastField<FBoolProperty>(Property)) {
    return Bool->GetPropertyValue_InContainer(Settings) ? 1 : 0;
  }
  if (const FByteProperty *Byte = CastField<FByteProperty>(Property)) {
    return Byte->GetPropertyValue_InContainer(Settings);
  }
  const FEnumProperty *EnumProperty = CastField<FEnumProperty>(Property);
  return EnumProperty ? EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Settings)) : 0;
}

void McpWriteSetting(FProperty *Property, URendererSettings *Settings, int64 Value) {
  if (FBoolProperty *Bool = CastField<FBoolProperty>(Property)) {
    Bool->SetPropertyValue_InContainer(Settings, Value != 0);
  } else if (FByteProperty *Byte = CastField<FByteProperty>(Property)) {
    Byte->SetPropertyValue_InContainer(Settings, static_cast<uint8>(Value));
  } else if (FEnumProperty *EnumProperty = CastField<FEnumProperty>(Property)) {
    EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Settings), Value);
  }
}

TSharedPtr<FJsonValue> McpSettingJson(const FMcpRenderSetting &Setting, const FProperty *Property, int64 Value) {
  if (Setting.Values.Num() == 0) {
    return MakeShared<FJsonValueBoolean>(Value != 0);
  }
  const UEnum *Enum = McpSettingEnum(Property);
  const FString Entry = Enum ? Enum->GetNameStringByValue(Value) : FString();
  for (const TPair<FString, FString> &Pair : Setting.Values) {
    if (Pair.Value == Entry) {
      return MakeShared<FJsonValueString>(Pair.Key);
    }
  }
  return MakeShared<FJsonValueString>(Entry);
}

struct FMcpRenderWrite {
  const FMcpRenderSetting *Setting = nullptr;
  FProperty *Property = nullptr;
  int64 Value = 0;
};
} // namespace

bool HandleRenderSettingsAction(const FString &LowerSub, const TSharedPtr<FJsonObject> &Payload,
                                const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
                                FString &ErrorCode) {
  if (LowerSub != TEXT("configure_rendering")) {
    return false;
  }
  URendererSettings *Settings = GetMutableDefault<URendererSettings>();
  TArray<FMcpRenderWrite> Writes;
  TArray<TSharedPtr<FJsonValue>> Unavailable;
  // Every value is checked before any is written, so a bad one leaves the project as it was.
  for (const FMcpRenderSetting &Setting : McpRenderSettings()) {
    if (!Payload.IsValid() || !Payload->HasField(Setting.Param)) {
      continue;
    }
    FProperty *Property = FindFProperty<FProperty>(URendererSettings::StaticClass(), Setting.Property);
    if (!Property) {
      Unavailable.Add(MakeShared<FJsonValueString>(Setting.Param));
      continue;
    }
    FMcpRenderWrite Write{&Setting, Property, 0};
    bool bFlag = false;
    FString Text;
    if (Setting.Values.Num() == 0 && Payload->TryGetBoolField(Setting.Param, bFlag)) {
      Write.Value = bFlag ? 1 : 0;
    } else if (Setting.Values.Num() > 0 && Payload->TryGetStringField(Setting.Param, Text)) {
      const TPair<FString, FString> *Match = Setting.Values.FindByPredicate(
          [&Text](const TPair<FString, FString> &Pair) { return McpPlainKey(Pair.Key) == McpPlainKey(Text); });
      const UEnum *Enum = McpSettingEnum(Property);
      Write.Value = Match && Enum ? Enum->GetValueByNameString(Match->Value) : INDEX_NONE;
      if (Write.Value == INDEX_NONE) {
        TArray<FString> Allowed;
        for (const TPair<FString, FString> &Pair : Setting.Values) {
          Allowed.Add(Pair.Key);
        }
        Message = FString::Printf(TEXT("%s '%s' is not one of %s."), Setting.Param, *Text, *FString::Join(Allowed, TEXT(", ")));
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
    } else {
      Message = FString::Printf(TEXT("%s takes %s."), Setting.Param, Setting.Values.Num() == 0 ? TEXT("true or false") : TEXT("a name"));
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
    Writes.Add(Write);
  }

  TArray<TSharedPtr<FJsonValue>> Changed;
  TArray<TSharedPtr<FJsonValue>> Restart;
  for (const FMcpRenderWrite &Write : Writes) {
    if (McpReadSetting(Write.Property, Settings) == Write.Value) {
      continue;
    }
    McpWriteSetting(Write.Property, Settings, Write.Value);
    Changed.Add(MakeShared<FJsonValueString>(Write.Setting->Param));
    // The console variable carries the setting into the running editor; a read-only one only changes on restart.
    IConsoleVariable *Variable = IConsoleManager::Get().FindConsoleVariable(*Write.Property->GetMetaData(TEXT("ConsoleVariable")));
    if (Variable && !Variable->TestFlags(ECVF_ReadOnly)) {
      Variable->Set(*LexToString(Write.Value), ECVF_SetByProjectSetting);
    }
    if (Write.Property->HasMetaData(TEXT("ConfigRestartRequired")) || !Variable || Variable->TestFlags(ECVF_ReadOnly)) {
      Restart.Add(MakeShared<FJsonValueString>(Write.Setting->Param));
    }
  }
  if (Changed.Num() > 0 && !Settings->TryUpdateDefaultConfigFile(FString(), false)) {
    Message = TEXT("The settings changed in the editor but DefaultEngine.ini could not be written (is it read-only or checked in?).");
    ErrorCode = TEXT("PERSIST_FAILED");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  const TSharedPtr<FJsonObject> Current = MakeShared<FJsonObject>();
  for (const FMcpRenderSetting &Setting : McpRenderSettings()) {
    if (const FProperty *Property = FindFProperty<FProperty>(URendererSettings::StaticClass(), Setting.Property)) {
      Current->SetField(Setting.Param, McpSettingJson(Setting, Property, McpReadSetting(Property, Settings)));
    }
  }
  Resp->SetObjectField(TEXT("settings"), Current);
  Resp->SetArrayField(TEXT("changed"), Changed);
  Resp->SetArrayField(TEXT("restartRequired"), Restart);
  if (Unavailable.Num() > 0) {
    Resp->SetArrayField(TEXT("unavailable"), Unavailable);
  }
  // Software Lumen traces mesh distance fields: with neither them nor hardware ray tracing it lights nothing.
  FString Method;
  bool bDistanceFields = true;
  bool bHardware = true;
  if (Current->TryGetStringField(TEXT("globalIllumination"), Method) && Method == TEXT("lumen") &&
      Current->TryGetBoolField(TEXT("meshDistanceFields"), bDistanceFields) && !bDistanceFields &&
      Current->TryGetBoolField(TEXT("hardwareRayTracing"), bHardware) && !bHardware) {
    Resp->SetStringField(TEXT("note"), TEXT("Lumen global illumination needs meshDistanceFields (or hardwareRayTracing) to light anything."));
  }
  McpHandlerUtils::MarkNoAssetsChanged(Resp);
  bSuccess = true;
  Message = Changed.Num() == 0 ? FString(TEXT("Rendering settings read; nothing changed."))
                               : FString::Printf(TEXT("Changed %d rendering setting(s); %d wait for an editor restart."), Changed.Num(), Restart.Num());
  return true;
}
} // namespace McpUiHandlers
