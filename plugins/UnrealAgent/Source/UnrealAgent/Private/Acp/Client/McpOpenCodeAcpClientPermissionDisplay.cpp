#include "Acp/Client/McpOpenCodeAcpClient.h"

FString FOpenCodeAcpClient::GetModelDisplayName(const FString& ModelId) const
{
    const FOpenCodeAcpModelOption* Option = ModelOptions.FindByPredicate([&ModelId](const FOpenCodeAcpModelOption& Candidate)
    {
        return Candidate.Id == ModelId;
    });

    return Option != nullptr ? Option->GetDisplayName() : ModelId;
}

FString FOpenCodeAcpClient::GetThinkingDisplayName(const FString& ThinkingId) const
{
    const FOpenCodeAcpThinkingOption* ThinkingOption = ThinkingOptions.FindByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });
    return ThinkingOption == nullptr ? ThinkingId : ThinkingOption->GetDisplayName();
}

FString FOpenCodeAcpClient::GetAgentDisplayName(const FString& AgentId) const
{
    const FOpenCodeAcpAgentOption* Option = AgentOptions.FindByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Candidate)
    {
        return Candidate.Id == AgentId;
    });

    if (AgentId == UnrealAgentId)
    {
        return TEXT("Unreal - Creator");
    }
    return Option != nullptr ? Option->GetDisplayName() : AgentId;
}
