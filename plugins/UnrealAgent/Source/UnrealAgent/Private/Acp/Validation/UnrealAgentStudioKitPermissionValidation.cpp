#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "Acp/Validation/UnrealAgentStudioKitPermissionJson.h"
#include "Acp/Validation/UnrealAgentStudioKitPermissionPolicies.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UnrealAgent::Validation
{
namespace
{
void AddPermissionTextCheck(
    FUnrealAgentValidationResult& Result,
    const FString& ConfigText,
    const FString& Source,
    const bool bRequireTopLevelPolicy)
{
    const FString NormalizedConfig = NormalizeOpenCodeJsonText(ConfigText);
    const int32 JsonStart = NormalizedConfig.Find(TEXT("{"));
    TSharedPtr<FJsonObject> ConfigObject;
    if (JsonStart == INDEX_NONE)
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config JSON is invalid: %s"),
            *Source));
        return;
    }
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(NormalizedConfig.Mid(JsonStart));
    if (!FJsonSerializer::Deserialize(Reader, ConfigObject)
        || !ConfigObject.IsValid())
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config JSON is invalid: %s"),
            *Source));
        return;
    }
    AddOpenCodePermissionConfigErrors(
        Result,
        ConfigObject,
        Source,
        bRequireTopLevelPolicy);
}
}

void AddOptionalOpenCodePermissionTextCheck(
    FUnrealAgentValidationResult& Result,
    const FString& ConfigText,
    const FString& Source)
{
    if (!ConfigText.IsEmpty())
    {
        AddPermissionTextCheck(Result, ConfigText, Source, false);
    }
}

void AddOpenCodePermissionCheck(
    FUnrealAgentValidationResult& Result,
    const FString& Path)
{
    FString ConfigText;
    if (!FFileHelper::LoadFileToString(ConfigText, *Path))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config is unreadable: %s"),
            *Path));
        return;
    }
    AddPermissionTextCheck(Result, ConfigText, Path, true);
}

void AddOptionalOpenCodePermissionCheck(
    FUnrealAgentValidationResult& Result,
    const FString& Path)
{
    if (!FPaths::FileExists(Path))
    {
        return;
    }
    FString ConfigText;
    if (!FFileHelper::LoadFileToString(ConfigText, *Path))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode config is unreadable: %s"),
            *Path));
        return;
    }
    AddPermissionTextCheck(Result, ConfigText, Path, false);
}
}
