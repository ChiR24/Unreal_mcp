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

bool FOpenCodeAcpClient::ParseAgentOptionsFromResult(const TSharedPtr<FJsonObject>& Result)
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
            if (ParseAgentOptionsFromConfigOption(Option))
            {
                return !CurrentAgent.IsEmpty();
            }
        }
    }

    return ParseFallbackAgents(Result);
}

bool FOpenCodeAcpClient::ParseAgentOptionsFromConfigOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid() || !IsAgentConfigOption(Option))
    {
        return false;
    }

    const FString OptionId = GetConfigOptionId(Option);
    AgentConfigId = OptionId.IsEmpty() ? TEXT("mode") : OptionId;

    const FString NewCurrentAgent = GetConfigOptionValue(Option);
    if (!NewCurrentAgent.IsEmpty())
    {
        CurrentAgent = NewCurrentAgent;
    }

    AgentOptions.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
    if (Option->TryGetArrayField(TEXT("options"), Options))
    {
        for (const TSharedPtr<FJsonValue>& AgentValue : *Options)
        {
            ParseAgentOption(AgentValue.IsValid() ? AgentValue->AsObject() : nullptr);
        }
    }

    return true;
}

void FOpenCodeAcpClient::ParseAgentOption(const TSharedPtr<FJsonObject>& Option)
{
    if (!Option.IsValid())
    {
        return;
    }

    FOpenCodeAcpAgentOption AgentOption;
    Option->TryGetStringField(TEXT("value"), AgentOption.Id);
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("agentId"), AgentOption.Id);
    }
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("modeId"), AgentOption.Id);
    }
    if (AgentOption.Id.IsEmpty())
    {
        Option->TryGetStringField(TEXT("id"), AgentOption.Id);
    }
    Option->TryGetStringField(TEXT("name"), AgentOption.Name);
    Option->TryGetStringField(TEXT("description"), AgentOption.Description);
    if (!AgentOption.Id.IsEmpty())
    {
        AgentOptions.Add(MoveTemp(AgentOption));
    }
}

bool FOpenCodeAcpClient::ParseFallbackAgents(const TSharedPtr<FJsonObject>& Result)
{
    const TSharedPtr<FJsonObject>* Agents = nullptr;
    if (!Result.IsValid()
        || ((!Result->TryGetObjectField(TEXT("agents"), Agents) || !Agents || !Agents->IsValid())
            && (!Result->TryGetObjectField(TEXT("modes"), Agents) || !Agents || !Agents->IsValid())))
    {
        return false;
    }

    FString NewCurrentAgent;
    const bool bHasCurrentAgent = ((*Agents)->TryGetStringField(TEXT("currentAgentId"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentAgent"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentModeId"), NewCurrentAgent)
        || (*Agents)->TryGetStringField(TEXT("currentMode"), NewCurrentAgent))
        && !NewCurrentAgent.IsEmpty();
    if (bHasCurrentAgent)
    {
        CurrentAgent = NewCurrentAgent;
    }

    const TArray<TSharedPtr<FJsonValue>>* AvailableAgents = nullptr;
    if (!(*Agents)->TryGetArrayField(TEXT("availableAgents"), AvailableAgents)
        && !(*Agents)->TryGetArrayField(TEXT("availableModes"), AvailableAgents))
    {
        return bHasCurrentAgent;
    }

    AgentConfigId = TEXT("mode");
    AgentOptions.Reset();
    for (const TSharedPtr<FJsonValue>& AgentValue : *AvailableAgents)
    {
        const TSharedPtr<FJsonObject> Agent = AgentValue.IsValid() ? AgentValue->AsObject() : nullptr;
        if (!Agent.IsValid())
        {
            continue;
        }

        FOpenCodeAcpAgentOption AgentOption;
        Agent->TryGetStringField(TEXT("agentId"), AgentOption.Id);
        if (AgentOption.Id.IsEmpty())
        {
            Agent->TryGetStringField(TEXT("modeId"), AgentOption.Id);
        }
        if (AgentOption.Id.IsEmpty())
        {
            Agent->TryGetStringField(TEXT("id"), AgentOption.Id);
        }
        Agent->TryGetStringField(TEXT("name"), AgentOption.Name);
        Agent->TryGetStringField(TEXT("description"), AgentOption.Description);
        if (!AgentOption.Id.IsEmpty())
        {
            AgentOptions.Add(MoveTemp(AgentOption));
        }
    }
    return bHasCurrentAgent;
}
