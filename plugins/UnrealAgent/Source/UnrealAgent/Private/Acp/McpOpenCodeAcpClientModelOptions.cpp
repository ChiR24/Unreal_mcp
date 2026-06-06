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

bool FOpenCodeAcpClient::ParseModelOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
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
            if (!Option.IsValid() || !IsModelConfigOption(Option))
            {
                continue;
            }

            const FString OptionId = GetConfigOptionId(Option);
            ModelConfigId = OptionId.IsEmpty() ? TEXT("model") : OptionId;
            const FString NewCurrentModel = GetConfigOptionValue(Option);
            const bool bHasCurrentModel = !NewCurrentModel.IsEmpty();
            if (bHasCurrentModel)
            {
                SetCurrentModel(NewCurrentModel);
            }
            ModelOptions.Reset();

            const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
            if (Option->TryGetArrayField(TEXT("options"), Options))
            {
                for (const TSharedPtr<FJsonValue>& ModelValue : *Options)
                {
                    ParseModelOption(ModelValue.IsValid() ? ModelValue->AsObject() : nullptr);
                }
            }
            return bHasCurrentModel;
        }
    }

    return ParseFallbackModels(Result);
}

void FOpenCodeAcpClient::ParseModelOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpModelOption ModelOption;
    Option->TryGetStringField(TEXT("value"), ModelOption.Id);
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), ModelOption.Id);
    }
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("optionId"), ModelOption.Id);
    }
    if (ModelOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("modelId"), ModelOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), ModelOption.Name);
    if (ModelOption.Name.IsEmpty())
    {
        Option->TryGetStringField(TEXT("label"), ModelOption.Name);
    }
    ModelOption.Provider = GetModelProviderField(Option);
    ModelOption.ContextWindowTokens = GetModelContextWindowTokens(Option);
    if (!ModelOption.Id.IsEmpty())
    {
        ModelOptions.Add(MoveTemp(ModelOption));
    }
}

bool FOpenCodeAcpClient::ParseFallbackModels(const TSharedPtr<FJsonObject>& Result)
{
    const TSharedPtr<FJsonObject>* Models = nullptr;
    if (!Result.IsValid() || !Result->TryGetObjectField(TEXT("models"), Models) || !Models || !Models->IsValid())
    {
        return false;
    }

    FString NewCurrentModel;
    const bool bHasCurrentModel = ((*Models)->TryGetStringField(TEXT("currentModelId"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("currentModel"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("currentValue"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("value"), NewCurrentModel)
        || (*Models)->TryGetStringField(TEXT("optionValue"), NewCurrentModel))
        && !NewCurrentModel.IsEmpty();
    if (bHasCurrentModel)
    {
        SetCurrentModel(NewCurrentModel);
    }

    const TArray<TSharedPtr<FJsonValue>>* AvailableModels = nullptr;
    if (!(*Models)->TryGetArrayField(TEXT("availableModels"), AvailableModels))
    {
        return bHasCurrentModel;
    }

    ModelConfigId = TEXT("model");
    ModelOptions.Reset();
    for (const TSharedPtr<FJsonValue>& ModelValue : *AvailableModels)
    {
        const TSharedPtr<FJsonObject> Model = ModelValue.IsValid() ? ModelValue->AsObject() : nullptr;
        if (!Model.IsValid())
        {
            continue;
        }

        FOpenCodeAcpModelOption ModelOption;
        Model->TryGetStringField(TEXT("modelId"), ModelOption.Id);
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("value"), ModelOption.Id);
        }
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("id"), ModelOption.Id);
        }
        if (ModelOption.Id.IsEmpty())
        {
            Model->TryGetStringField(TEXT("optionId"), ModelOption.Id);
        }
        Model->TryGetStringField(TEXT("name"), ModelOption.Name);
        if (ModelOption.Name.IsEmpty())
        {
            Model->TryGetStringField(TEXT("label"), ModelOption.Name);
        }
        ModelOption.Provider = GetModelProviderField(Model);
        ModelOption.ContextWindowTokens = GetModelContextWindowTokens(Model);
        if (!ModelOption.Id.IsEmpty())
        {
            ModelOptions.Add(MoveTemp(ModelOption));
        }
    }
    return bHasCurrentModel;
}
