#pragma once

#include "CoreMinimal.h"

struct MCPAUTOMATIONBRIDGE_API FMcpAutomationBridgeToolSubActionCatalogEntry
{
    FString Name;
    FString Summary;
};

struct MCPAUTOMATIONBRIDGE_API FMcpAutomationBridgeToolCatalogEntry
{
    FString ToolName;
    FString Category;
    FString Summary;
    bool bPublic = true;
    TArray<FMcpAutomationBridgeToolSubActionCatalogEntry> SubActions;
};

MCPAUTOMATIONBRIDGE_API const TArray<FMcpAutomationBridgeToolCatalogEntry> &GetMcpAutomationBridgeToolCatalog();
MCPAUTOMATIONBRIDGE_API TArray<FMcpAutomationBridgeToolCatalogEntry> GetPublicMcpAutomationBridgeToolCatalog();
MCPAUTOMATIONBRIDGE_API const FMcpAutomationBridgeToolCatalogEntry *FindMcpAutomationBridgeToolCatalogEntry(const FString &ToolName);
MCPAUTOMATIONBRIDGE_API int32 CountPublicMcpAutomationBridgeTools();
MCPAUTOMATIONBRIDGE_API int32 CountPublicMcpAutomationBridgeToolActions();