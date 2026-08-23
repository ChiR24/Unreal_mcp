#include "Domains/Lighting/McpAutomationBridge_LightingBuildCompatibility.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{
namespace
{

constexpr TCHAR LightingBuildOptionsSection[] = TEXT("LightingBuildOptions");

struct FStoredBoolSetting
{
    const TCHAR* Key;
    bool bWasPresent = false;
    bool Value = false;
};

void CaptureBoolSetting(FStoredBoolSetting& Setting)
{
    Setting.bWasPresent = GConfig->GetBool(
        LightingBuildOptionsSection,
        Setting.Key,
        Setting.Value,
        GEditorPerProjectIni);
}

void RestoreBoolSetting(const FStoredBoolSetting& Setting)
{
    if (Setting.bWasPresent)
    {
        GConfig->SetBool(
            LightingBuildOptionsSection,
            Setting.Key,
            Setting.Value,
            GEditorPerProjectIni);
        return;
    }

    GConfig->RemoveKey(
        LightingBuildOptionsSection,
        Setting.Key,
        GEditorPerProjectIni);
}

}

bool RunLegacyLightingBuild(
    UWorld& World,
    const ELightingBuildQuality Quality)
{
    FStoredBoolSetting ScopeSettings[] = {
        {TEXT("OnlyBuildSelected")},
        {TEXT("OnlyBuildCurrentLevel")},
        {TEXT("OnlyBuildSelectedLevels")},
        {TEXT("OnlyBuildVisibility")},
    };
    for (FStoredBoolSetting& Setting : ScopeSettings)
    {
        CaptureBoolSetting(Setting);
        GConfig->SetBool(
            LightingBuildOptionsSection,
            Setting.Key,
            false,
            GEditorPerProjectIni);
    }

    int32 OldQualityLevel = 0;
    const bool bHadQualityLevel = GConfig->GetInt(
        LightingBuildOptionsSection,
        TEXT("QualityLevel"),
        OldQualityLevel,
        GEditorPerProjectIni);
    GConfig->SetInt(
        LightingBuildOptionsSection,
        TEXT("QualityLevel"),
        static_cast<int32>(Quality),
        GEditorPerProjectIni);

    const bool bBuildSucceeded =
        FEditorBuildUtils::EditorBuild(
            &World,
            FBuildOptions::BuildLighting,
            false);

    for (const FStoredBoolSetting& Setting : ScopeSettings)
    {
        RestoreBoolSetting(Setting);
    }
    if (bHadQualityLevel)
    {
        GConfig->SetInt(
            LightingBuildOptionsSection,
            TEXT("QualityLevel"),
            OldQualityLevel,
            GEditorPerProjectIni);
    }
    else
    {
        GConfig->RemoveKey(
            LightingBuildOptionsSection,
            TEXT("QualityLevel"),
            GEditorPerProjectIni);
    }

    return bBuildSucceeded;
}

}
#endif
