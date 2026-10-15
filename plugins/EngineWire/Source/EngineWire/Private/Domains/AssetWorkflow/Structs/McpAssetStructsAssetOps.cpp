#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"

// Thin dispatcher for struct asset operations. The action handlers formerly
// colocated in this file now live in the dedicated implementation files below
// and are textually included so the module still compiles them as one
// translation unit. Each helper returns true when it handled the action.
#define MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
#include "McpAssetStructsAssetOpsDelete.cpp"
#include "McpAssetStructsAssetOpsRename.cpp"
#include "McpAssetStructsAssetOpsRefresh.cpp"
#undef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL


bool HandleStructAssetActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (HandleStructAssetAction_Delete(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleStructAssetAction_Rename(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandleStructAssetAction_Refresh(Bridge, RequestId, Action, Payload, RequestingSocket)) return true;
    return false;
}

