#include "McpAutomationBridgeGlobals.h"

TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;
TMap<FString, FString> GBlueprintExistCacheNormalized;
TMap<FString, double> GBlueprintExistCacheTs;
double GBlueprintExistCacheTTLSeconds = 10.0;

TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>> GBlueprintExistsInflight;
TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>> GBlueprintCreateInflight;
TMap<FString, double> GBlueprintCreateInflightTs;
FCriticalSection GBlueprintCreateMutex;
double GBlueprintCreateStaleTimeoutSec = 60.0;
TSet<FString> GBlueprintBusySet;

TMap<FString, TSharedPtr<FJsonObject>> GSequenceRegistry;
FString GCurrentSequencePath;

FCriticalSection GPythonExecMutex;
TMap<FString, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>> GPythonExecInflight;
