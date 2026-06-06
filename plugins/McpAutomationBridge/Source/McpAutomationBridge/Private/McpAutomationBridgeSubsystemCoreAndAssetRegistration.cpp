#include "McpAutomationBridgeSubsystem.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterCoreAndAssetHandlers()
{
    MCP_REGISTER_DIRECT("execute_editor_function", HandleExecuteEditorFunction);
    MCP_REGISTER_DIRECT("set_object_property", HandleSetObjectProperty);
    MCP_REGISTER_DIRECT("get_object_property", HandleGetObjectProperty);
    MCP_REGISTER_DIRECT("array_append", HandleArrayAppend);
    MCP_REGISTER_DIRECT("array_remove", HandleArrayRemove);
    MCP_REGISTER_DIRECT("array_insert", HandleArrayInsert);
    MCP_REGISTER_DIRECT("array_get_element", HandleArrayGetElement);
    MCP_REGISTER_DIRECT("array_set_element", HandleArraySetElement);
    MCP_REGISTER_DIRECT("array_clear", HandleArrayClear);
    MCP_REGISTER_DIRECT("map_set_value", HandleMapSetValue);
    MCP_REGISTER_DIRECT("map_get_value", HandleMapGetValue);
    MCP_REGISTER_DIRECT("map_remove_key", HandleMapRemoveKey);
    MCP_REGISTER_DIRECT("map_has_key", HandleMapHasKey);
    MCP_REGISTER_DIRECT("map_get_keys", HandleMapGetKeys);
    MCP_REGISTER_DIRECT("map_clear", HandleMapClear);
    MCP_REGISTER_DIRECT("set_add", HandleSetAdd);
    MCP_REGISTER_DIRECT("set_remove", HandleSetRemove);
    MCP_REGISTER_DIRECT("set_contains", HandleSetContains);
    MCP_REGISTER_DIRECT("set_clear", HandleSetClear);
    MCP_REGISTER_DIRECT("get_asset_references", HandleGetAssetReferences);
    MCP_REGISTER_DIRECT("get_asset_dependencies", HandleGetAssetDependencies);
    MCP_REGISTER_DIRECT("fixup_redirectors", HandleFixupRedirectors);
    MCP_REGISTER_DIRECT("source_control_checkout", HandleSourceControlCheckout);
    MCP_REGISTER_DIRECT("source_control_submit", HandleSourceControlSubmit);
    MCP_REGISTER_DIRECT("bulk_rename_assets", HandleBulkRenameAssets);
    MCP_REGISTER_DIRECT("bulk_delete_assets", HandleBulkDeleteAssets);
    MCP_REGISTER_DIRECT("generate_thumbnail", HandleGenerateThumbnail);
    MCP_REGISTER_DIRECT("create_landscape", HandleCreateLandscape);
    MCP_REGISTER_DIRECT("create_procedural_terrain", HandleCreateProceduralTerrain);
    MCP_REGISTER_DIRECT("create_landscape_grass_type", HandleCreateLandscapeGrassType);
    MCP_REGISTER_DIRECT("sculpt_landscape", HandleSculptLandscape);
    MCP_REGISTER_DIRECT("set_landscape_material", HandleSetLandscapeMaterial);
    MCP_REGISTER_DIRECT("modify_heightmap", HandleModifyHeightmap);
    MCP_REGISTER_DIRECT("edit_landscape", HandleEditLandscape);
    MCP_REGISTER_DIRECT("add_foliage_type", HandleAddFoliageType);
    MCP_REGISTER_DIRECT("create_procedural_foliage", HandleCreateProceduralFoliage);
    MCP_REGISTER_DIRECT("paint_foliage", HandlePaintFoliage);
    MCP_REGISTER_DIRECT("add_foliage_instances", HandleAddFoliageInstances);
    MCP_REGISTER_DIRECT("remove_foliage", HandleRemoveFoliage);
    MCP_REGISTER_DIRECT("get_foliage_instances", HandleGetFoliageInstances);
}

#undef MCP_REGISTER_DIRECT
