#include "Domains/AssetWorkflow/Structs/McpAssetStructsAnalysis.h"

#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"


bool HandleStructAnalysisActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

