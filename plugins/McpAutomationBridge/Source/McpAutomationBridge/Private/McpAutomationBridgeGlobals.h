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

extern FCriticalSection GPythonExecMutex;
extern TMap<FString, TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>> GPythonExecInflight;
