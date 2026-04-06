#pragma once

#include "CoreMinimal.h"

struct MCPAUTOMATIONBRIDGE_API FMcpAutomationBridgeToolSubActionCatalogEntry
{
    FString Name;
    FString Summary;
    bool bEditorOnly = true;
    bool bRequiresLiveEditor = false;
    bool bRequiresAssetEditor = false;
    FString InteractionModel;
    FString LimitationNote;
};

struct MCPAUTOMATIONBRIDGE_API FMcpAutomationBridgeToolCatalogEntry
{
    FString ToolName;
    FString Category;
    FString Summary;
    bool bPublic = true;
    TArray<FMcpAutomationBridgeToolSubActionCatalogEntry> SubActions;
};

/** @brief Contract: returns the full bridge-owned tool catalog, including non-public entries used for internal validation. */
MCPAUTOMATIONBRIDGE_API const TArray<FMcpAutomationBridgeToolCatalogEntry> &GetMcpAutomationBridgeToolCatalog();
/** @brief Contract: returns only the catalog entries that are part of the published MCP surface. */
MCPAUTOMATIONBRIDGE_API TArray<FMcpAutomationBridgeToolCatalogEntry> GetPublicMcpAutomationBridgeToolCatalog();
/** @brief Contract: resolves a catalog entry by exact tool name so registration and diagnostics can share one source of truth. */
MCPAUTOMATIONBRIDGE_API const FMcpAutomationBridgeToolCatalogEntry *FindMcpAutomationBridgeToolCatalogEntry(const FString &ToolName);
/** @brief Contract: counts the currently public tools exposed by the bridge catalog. */
MCPAUTOMATIONBRIDGE_API int32 CountPublicMcpAutomationBridgeTools();
/** @brief Contract: counts the currently public tool actions, treating tools without subactions as one action each. */
MCPAUTOMATIONBRIDGE_API int32 CountPublicMcpAutomationBridgeToolActions();