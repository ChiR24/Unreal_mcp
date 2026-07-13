#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

// Thin dispatcher for struct asset operations. The action handlers formerly
// colocated in this file now live in the dedicated implementation files below
// and are textually included so the module still compiles them as one
// translation unit. Each helper returns true when it handled the action.
#define MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
#include "McpAutomationBridge_AssetWorkflowStructsAssetOpsDelete.cpp"
#include "McpAutomationBridge_AssetWorkflowStructsAssetOpsRename.cpp"
#include "McpAutomationBridge_AssetWorkflowStructsAssetOpsRefresh.cpp"
#undef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

#if WITH_EDITOR

bool HandleStructAssetActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (HandleStructAssetAction_Delete(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleStructAssetAction_Rename(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleStructAssetAction_Refresh(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    return false;
}

#endif // WITH_EDITOR
