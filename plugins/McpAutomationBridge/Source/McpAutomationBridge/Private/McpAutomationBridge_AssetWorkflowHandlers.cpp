// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, check for a subAction in
  // the payload
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) &&
        !SubAction.IsEmpty()) {
      Lower = SubAction.ToLower();
    }
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers
  // CRITICAL: These actions must match what TS sends as 'action' (not just 'subAction')
  // When TS calls executeAutomationRequest(tools, 'search_assets', {...}), Action='search_assets'

  // Asset Operations
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("duplicate") || Lower == TEXT("duplicate_asset"))
    return HandleDuplicateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("rename") || Lower == TEXT("rename_asset"))
    return HandleRenameAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("move") || Lower == TEXT("move_asset"))
    return HandleMoveAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("delete") || Lower == TEXT("delete_asset") || Lower == TEXT("delete_assets"))
    return HandleDeleteAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_folder"))
    return HandleCreateFolder(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material"))
    return HandleCreateMaterial(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_material_instance"))
    return HandleCreateMaterialInstance(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_render_target"))
    return HandleManageTextureAction(RequestId, TEXT("manage_texture"), Payload, RequestingSocket);
  if (Lower == TEXT("get_dependencies"))
    return HandleGetDependencies(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_asset_graph"))
    return HandleGetAssetGraph(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_tags"))
    return HandleSetTags(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("set_metadata"))
    return HandleSetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_metadata"))
    return HandleGetMetadata(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("validate"))
    return HandleValidateAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list") || Lower == TEXT("list_assets"))
    return HandleListAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("generate_report"))
    return HandleGenerateReport(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("create_thumbnail") || Lower == TEXT("generate_thumbnail"))
    return HandleGenerateThumbnail(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("add_material_parameter"))
    return HandleAddMaterialParameter(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_instances"))
    return HandleListMaterialInstances(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("reset_instance_parameters"))
    return HandleResetInstanceParameters(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("exists"))
    return HandleDoesAssetExist(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_stats"))
    return HandleGetMaterialStats(RequestId, Payload, RequestingSocket);

  // Search (CRITICAL: search_assets must be dispatched - was missing causing timeouts)
  if (Lower == TEXT("search_assets"))
    return HandleSearchAssets(RequestId, Action, Payload, RequestingSocket);

  // Bulk Operations
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Action, Payload, RequestingSocket);

  // Source Control
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("get_source_control_state"))
    return HandleGetSourceControlState(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Action, Payload, RequestingSocket);

  // Graph & Analysis
  if (Lower == TEXT("analyze_graph"))
    return HandleAnalyzeGraph(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Action, Payload, RequestingSocket);

  // Material Authoring
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Action, Payload, RequestingSocket);
  if (Lower == TEXT("rebuild_material"))
    return HandleRebuildMaterial(RequestId, Action, Payload, RequestingSocket);

  return false;
}


// ============================================================================
// 1. FIXUP REDIRECTORS
// ============================================================================
