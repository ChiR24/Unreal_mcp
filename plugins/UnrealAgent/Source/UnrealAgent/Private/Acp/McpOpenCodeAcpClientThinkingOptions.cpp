#include "McpOpenCodeAcpClient.h"
#include "McpOpenCodeAcpClientPrivate.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

bool FOpenCodeAcpClient::ParseThinkingOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ConfigOptions = nullptr;
    if (Result->TryGetArrayField(TEXT("configOptions"), ConfigOptions))
    {
        for (const TSharedPtr<FJsonValue>& OptionValue : *ConfigOptions)
        {
            const TSharedPtr<FJsonObject> Option = OptionValue.IsValid() ? OptionValue->AsObject() : nullptr;
            if (ParseThinkingOptionsFromConfigOption(Option))
            {
                return !CurrentThinking.IsEmpty();
            }
        }
    }

    return false;
}

bool FOpenCodeAcpClient::ParseThinkingOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid() || !IsThinkingConfigOption(Option))
    {
        return false;
    }

    const FString OptionId = GetConfigOptionId(Option);
    ThinkingConfigId = OptionId.IsEmpty() ? TEXT("reasoning") : OptionId;

    const FString NewCurrentThinking = GetConfigOptionValue(Option);
    if (!NewCurrentThinking.IsEmpty())
    {
        CurrentThinking = NewCurrentThinking;
    }

    ThinkingOptions.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Option->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& ThinkingValue : *Options)
        {
            ParseThinkingOption(ThinkingValue.IsValid() ? ThinkingValue->AsObject() : nullptr);
        }
    }

    return true;
}

void FOpenCodeAcpClient::ParseThinkingOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpThinkingOption ThinkingOption;
    Option->TryGetStringField(TEXT("value"), ThinkingOption.Id);
    if (ThinkingOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), ThinkingOption.Id);
    }
    if (ThinkingOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("optionId"), ThinkingOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), ThinkingOption.Name);
    if (ThinkingOption.Name.IsEmpty())
    {
        Option->TryGetStringField(TEXT("label"), ThinkingOption.Name);
    }
    Option->TryGetStringField(TEXT("description"), ThinkingOption.Description);
    if (!ThinkingOption.Id.IsEmpty())
    {
        ThinkingOptions.Add(MoveTemp(ThinkingOption));
    }
}
