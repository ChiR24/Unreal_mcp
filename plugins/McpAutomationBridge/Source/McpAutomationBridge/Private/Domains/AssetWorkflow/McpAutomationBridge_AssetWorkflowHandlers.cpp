// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "MCP/Routing/McpConsolidatedActionRouting.h"

// Struct ecosystem (issue #struct-ecosystem) — Wave 1 handler shard headers.
#include "Domains/AssetWorkflow/DataTables/Shared.h"
#include "Domains/AssetWorkflow/Enums/Shared.h"
namespace McpStructProperty
{
    bool HandleStructPropertyAction(FString Action, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult);
}

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Lower = Action.ToLower();

  // If the action is the generic "manage_asset" tool, resolve the concrete
  // action from the payload. This read `subAction` ONLY, but the native gateway
  // writes the concrete action to `action` (McpNativeGatewayValidation.cpp sets
  // Arguments["action"] = LegacyAction and never sets subAction), so every
  // native manage_asset call failed to promote, fell through this whole
  // dispatch table, and was answered by an unrelated domain further down the
  // chain — analyze_graph came back as "Unknown manage_sessions action".
  // GetPayloadSubAction is the repo's shared reader for exactly this: subAction
  // first, then action, normalized. It is already what the manage_asset routing
  // registration uses, so this makes the two agree.
  if (Lower == TEXT("manage_asset") && Payload.IsValid()) {
    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);
    if (!SubAction.IsEmpty() && SubAction != TEXT("manage_asset")) {
      Lower = SubAction;
    }
  }

  if (Lower.IsEmpty())
    return false;

  // Dispatch to specific handlers
  // CRITICAL: These actions must match what TS sends as 'action' (not just 'subAction')
  // When TS calls executeAutomationRequest(tools, 'search_assets', {...}), Action='search_assets'
  //
  // Leaf handlers that take an action parameter are passed `Lower`, never
  // `Action`. `Lower` is the RESOLVED concrete action; `Action` is still the
  // parent verb ("manage_asset") whenever the gateway dispatched by tool name.
  // Seventeen of these passed `Action`, so each leaf re-checked its own name,
  // saw "manage_asset", and DECLINED the request it had just been selected for
  // — after which the request fell through to whatever catch-all claimed it
  // next (analyze_graph was answered by manage_sessions, then by
  // manage_level_structure). Selection and hand-off must use the same string.

  // Asset Operations
  if (Lower == TEXT("import"))
    return HandleImportAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_content_sources"))
    return HandleListContentSources(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("migrate_assets"))
    return HandleMigrateAssets(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_fab_downloads"))
    return HandleListFabDownloads(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_fab_library"))
    return HandleListFabLibrary(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("download_fab_asset"))
    return HandleDownloadFabAsset(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("add_fab_asset_to_project"))
    return HandleAddFabAssetToProject(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("search_fab_listings"))
    return HandleSearchFabListings(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("get_fab_listing_details"))
    return HandleGetFabListingDetails(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("list_megascans_library"))
    return HandleListMegascansLibrary(RequestId, Payload, RequestingSocket);
  if (Lower == TEXT("import_megascans_asset"))
    return HandleImportMegascansAsset(RequestId, Payload, RequestingSocket);
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
    return HandleSearchAssets(RequestId, Lower, Payload, RequestingSocket);

  // Bulk Operations
  if (Lower == TEXT("fixup_redirectors"))
    return HandleFixupRedirectors(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_rename"))
    return HandleBulkRenameAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("bulk_delete"))
    return HandleBulkDeleteAssets(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("generate_lods"))
    return HandleGenerateLODs(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("nanite_rebuild_mesh"))
    return HandleNaniteRebuildMesh(RequestId, Lower, Payload, RequestingSocket);

  // Source Control
  if (Lower == TEXT("source_control_checkout"))
    return HandleSourceControlCheckout(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_submit"))
    return HandleSourceControlSubmit(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_source_control_state"))
    return HandleGetSourceControlState(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("source_control_enable"))
    return HandleSourceControlEnable(RequestId, Lower, Payload, RequestingSocket);

  // Graph & Analysis
  if (Lower == TEXT("analyze_graph"))
    return HandleAnalyzeGraph(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("find_by_tag"))
    return HandleFindByTag(RequestId, Lower, Payload, RequestingSocket);

  // Material Authoring
  if (Lower == TEXT("add_material_node"))
    return HandleAddMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("connect_material_pins"))
    return HandleConnectMaterialPins(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("remove_material_node"))
    return HandleRemoveMaterialNode(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("break_material_connections"))
    return HandleBreakMaterialConnections(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("get_material_node_details"))
    return HandleGetMaterialNodeDetails(RequestId, Lower, Payload, RequestingSocket);
  if (Lower == TEXT("rebuild_material"))
    return HandleRebuildMaterial(RequestId, Lower, Payload, RequestingSocket);

  // Struct Authoring (first-class Blueprint Struct support, issue #510)
  if (Lower == TEXT("create_struct") || Lower == TEXT("get_struct") ||
      Lower == TEXT("read_struct") || Lower == TEXT("list_struct_members") ||
      Lower == TEXT("add_struct_member") || Lower == TEXT("remove_struct_member") ||
      Lower == TEXT("rename_struct_member") || Lower == TEXT("set_struct_member_type") ||
      Lower == TEXT("reorder_struct_members") || Lower == TEXT("set_struct_member_default") ||
      Lower == TEXT("set_struct_member_metadata") || Lower == TEXT("compare_structs") ||
      Lower == TEXT("search_struct_usage") || Lower == TEXT("recompile_struct") ||
      Lower == TEXT("rename_struct") || Lower == TEXT("duplicate_struct") ||
      Lower == TEXT("delete_struct") || Lower == TEXT("refresh_struct_dependencies") ||
      Lower == TEXT("list_structs") || Lower == TEXT("export_struct") || Lower == TEXT("import_struct"))
    return HandleStructAction(RequestId, Lower, Payload, RequestingSocket);

  // Struct ecosystem — DataTable (issue #struct-ecosystem)
  if (Lower == TEXT("create_data_table") || Lower == TEXT("set_data_table_row_struct") ||
      Lower == TEXT("create_row_struct") || Lower == TEXT("get_row_struct") ||
      Lower == TEXT("set_struct_as_row_struct") || Lower == TEXT("add_data_table_row") ||
      Lower == TEXT("get_data_table_row") || Lower == TEXT("update_data_table_row") ||
      Lower == TEXT("delete_data_table_row") || Lower == TEXT("list_data_table_rows") ||
      Lower == TEXT("import_data_table_rows") || Lower == TEXT("clear_data_table_rows"))
  {
    TSharedPtr<FJsonObject> Result;
    if (HandleDataTableAction(Lower, Payload, Result))
    {
      // A DataTable action that fails validation still returns true (it fills
      // Result with an error object via McpDataTableMakeError). Propagate that
      // failure to the caller instead of claiming success (issue
      // #struct-ecosystem [22]). An error result only carries the "error" /
      // "errorCode" fields and never a populated success payload, so keying the
      // success flag off the presence of an "error" field is reliable.
      FString Err;
      const bool bOk = !(Result.IsValid() &&
                         Result->TryGetStringField(TEXT("error"), Err) &&
                         !Err.IsEmpty());
      SendAutomationResponse(RequestingSocket, RequestId, bOk,
          bOk ? TEXT("DataTable action completed") : TEXT("DataTable action failed"),
          Result);
      return true;
    }
    return false;
  }

  // Struct ecosystem — Enum
  if (Lower == TEXT("create_enum") || Lower == TEXT("delete_enum") ||
      Lower == TEXT("get_enum") || Lower == TEXT("add_enum_value") ||
      Lower == TEXT("remove_enum_value") || Lower == TEXT("rename_enum_value") ||
      Lower == TEXT("reorder_enum_values") || Lower == TEXT("set_enum_value_metadata") ||
      Lower == TEXT("split_enum"))
  {
    TSharedPtr<FJsonObject> Result;
    if (HandleEnumAction(Lower, Payload, Result))
    {
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Enum action completed"), Result);
      return true;
    }
    return false;
  }

  // Struct ecosystem — FInstancedStruct
  if (Lower == TEXT("get_instanced_struct_property") || Lower == TEXT("set_instanced_struct_property"))
  {
    TSharedPtr<FJsonObject> Result;
    if (McpStructProperty::HandleStructPropertyAction(Lower, Payload, Result))
    {
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("FInstancedStruct property action completed"), Result);
      return true;
    }
    return false;
  }

  return false;
}

