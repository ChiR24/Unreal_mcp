#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsAnalysis.h"

#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR

bool HandleStructAnalysisActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("compare_structs"))
    {
        return HandleStructAnalysisCompare(Bridge, RequestId, Payload, RequestingSocket);
    }

    if (Lower == TEXT("search_struct_usage"))
    {
        return HandleStructAnalysisSearchUsage(Bridge, RequestId, Payload, RequestingSocket);
    }

    if (Lower == TEXT("recompile_struct"))
    {
        return HandleStructAnalysisRecompile(Bridge, RequestId, Payload, RequestingSocket);
    }

    return false;
}

#endif // WITH_EDITOR
