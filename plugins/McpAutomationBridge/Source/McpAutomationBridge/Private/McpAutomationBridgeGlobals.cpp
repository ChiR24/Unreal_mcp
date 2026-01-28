#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"

TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>>
    GBlueprintExistsInflight;
TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>>
    GBlueprintCreateInflight;
TMap<FString, double> GBlueprintCreateInflightTs;
FCriticalSection GBlueprintCreateMutex;
double GBlueprintCreateStaleTimeoutSec = 60.0;
TSet<FString> GBlueprintBusySet;
TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;

TMap<FString, TSharedPtr<FJsonObject>> GSequenceRegistry;
FString GCurrentSequencePath;

TMap<FString, TSharedPtr<FJsonObject>> GNiagaraRegistry;

// Recent asset save tracking (throttle across plugin to avoid frequent
// SavePackage calls)
TMap<FString, double> GRecentAssetSaveTs;
FCriticalSection GRecentAssetSaveMutex;
double GRecentAssetSaveThrottleSeconds = 0.5;
