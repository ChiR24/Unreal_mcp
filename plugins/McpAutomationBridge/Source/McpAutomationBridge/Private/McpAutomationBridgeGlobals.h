// Shared globals for McpAutomationBridge plugin
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

extern TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;
extern TMap<FString, FString> GBlueprintExistCacheNormalized;
extern TMap<FString, double> GBlueprintExistCacheTs;
extern double GBlueprintExistCacheTTLSeconds;

extern TMap<FString, TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>> GBlueprintExistsInflight;
extern TMap<FString, TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>> GBlueprintCreateInflight;
extern TMap<FString, double> GBlueprintCreateInflightTs;
extern FCriticalSection GBlueprintCreateMutex;
extern double GBlueprintCreateStaleTimeoutSec;
extern TSet<FString> GBlueprintBusySet;

extern TMap<FString, TSharedPtr<FJsonObject>> GSequenceRegistry;
extern FString GCurrentSequencePath;

// Lightweight registry used for created Niagara systems when running in
// fast-mode or when native Niagara factories are not available. Tests and
// higher-level tooling may rely on a plugin-side record of created
// Niagara assets even when on-disk creation is not possible.
extern TMap<FString, TSharedPtr<FJsonObject>> GNiagaraRegistry;

extern FCriticalSection GPythonExecMutex;
extern TMap<FString, TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>> GPythonExecInflight;

// Recent asset save tracking to throttle frequent SaveLoadedAsset calls
extern TMap<FString, double> GRecentAssetSaveTs;
extern FCriticalSection GRecentAssetSaveMutex;
extern double GRecentAssetSaveThrottleSeconds;
