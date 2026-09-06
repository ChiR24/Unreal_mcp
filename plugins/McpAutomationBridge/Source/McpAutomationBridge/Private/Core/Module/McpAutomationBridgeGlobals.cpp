#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"

TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>>
    GBlueprintCreateInflight;
TMap<FString, double> GBlueprintCreateInflightTs;
FCriticalSection GBlueprintCreateMutex;
TSet<FString> GBlueprintBusySet;
TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;

FString GCurrentSequencePath;

// Recent asset save tracking (throttle across plugin to avoid frequent
// SavePackage calls)
TMap<FString, double> GRecentAssetSaveTs;
FCriticalSection GRecentAssetSaveMutex;
double GRecentAssetSaveThrottleSeconds = 0.5;
