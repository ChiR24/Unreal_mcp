#include "McpAutomationBridgeSubsystem.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

// Custom authoring tools (ported): data tables, gameplay tags, data assets,
// layers, string tables, anim notifies, blueprint interfaces, physics materials.
void UMcpAutomationBridgeSubsystem::RegisterCustomToolingHandlers()
{
    MCP_REGISTER_DIRECT("manage_data_table", HandleManageDataTableAction);
    MCP_REGISTER_DIRECT("manage_gameplay_tags", HandleGameplayTags);
    MCP_REGISTER_DIRECT("manage_data_asset", HandleManageDataAssetAction);
    MCP_REGISTER_DIRECT("manage_layers", HandleManageLayersAction);
    MCP_REGISTER_DIRECT("manage_string_table", HandleManageStringTableAction);
    MCP_REGISTER_DIRECT("manage_anim_notify", HandleManageAnimNotifyAction);
    MCP_REGISTER_DIRECT("manage_blueprint_interface", HandleManageBlueprintInterfaceAction);
}

#undef MCP_REGISTER_DIRECT
