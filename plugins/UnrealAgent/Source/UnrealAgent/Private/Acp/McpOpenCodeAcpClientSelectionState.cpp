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

void FOpenCodeAcpClient::HandleModelUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewModel = GetConfigOptionValue(Update);
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("modelId"));
    }
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("currentModelId"));
    }
    if (NewModel.IsEmpty())
    {
        NewModel = GetStringFieldOrEmpty(Update, TEXT("currentModel"));
    }

    if (!NewModel.IsEmpty() && (OptionId.IsEmpty() || OptionId == ModelConfigId || IsModelConfigOption(Update)))
    {
        SetCurrentModel(NewModel);
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::SetCurrentModel(const FString& NewModel)
{
    if (NewModel.IsEmpty())
    {
        return;
    }

    if (NewModel != CurrentModel)
    {
        ContextWindowUsedTokens = 0;
        ContextWindowSizeTokens = 0;
    }
    CurrentModel = NewModel;
}

bool FOpenCodeAcpClient::TrySelectDefaultAgent()
{
    if (CurrentAgent == UnrealAgentId || !CanSelectAgent())
    {
        return false;
    }

    const FOpenCodeAcpAgentOption* UnrealAgentOption = AgentOptions.FindByPredicate([](const FOpenCodeAcpAgentOption& Option)
    {
        return Option.Id == UnrealAgentId;
    });

    if (UnrealAgentOption == nullptr)
    {
        return false;
    }

    SetAgent(UnrealAgentOption->Id);
    return SetAgentRequestId != INDEX_NONE;
}

void FOpenCodeAcpClient::HandleThinkingUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewThinking = GetConfigOptionValue(Update);
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("thinkingId"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("currentThinking"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("reasoning"));
    }
    if (NewThinking.IsEmpty())
    {
        NewThinking = GetStringFieldOrEmpty(Update, TEXT("currentReasoning"));
    }

    if (!NewThinking.IsEmpty() && (OptionId.IsEmpty() || OptionId == ThinkingConfigId || IsThinkingConfigOption(Update)))
    {
        CurrentThinking = NewThinking;
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::HandleAgentUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    const FString OptionId = GetConfigOptionId(Update);
    FString NewAgent = GetConfigOptionValue(Update);
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("agentId"));
    }
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("currentAgentId"));
    }
    if (NewAgent.IsEmpty())
    {
        NewAgent = GetStringFieldOrEmpty(Update, TEXT("currentAgent"));
    }

    if (!NewAgent.IsEmpty() && (OptionId.IsEmpty() || OptionId == AgentConfigId || OptionId == TEXT("agent") || OptionId == TEXT("mode")))
    {
        CurrentAgent = NewAgent;
        OnModelsChanged.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::HandleUsageUpdate(const TSharedPtr<FJsonObject>& Update)
{
    if (!Update.IsValid())
    {
        return;
    }

    double UsedTokens = 0.0;
    double SizeTokens = 0.0;
    if (!Update->TryGetNumberField(TEXT("used"), UsedTokens)
        || !Update->TryGetNumberField(TEXT("size"), SizeTokens)
        || UsedTokens < 0.0
        || SizeTokens <= 0.0)
    {
        return;
    }

    ContextWindowUsedTokens = FMath::Max(0, FMath::RoundToInt(UsedTokens));
    ContextWindowSizeTokens = FMath::Max(1, FMath::RoundToInt(SizeTokens));
    OnModelsChanged.ExecuteIfBound();
}
