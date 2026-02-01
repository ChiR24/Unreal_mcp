// Shared globals for McpAutomationBridge plugin
#pragma once

// Workaround for UE 5.0 engine headers using __has_feature (Clang-specific) with MSVC
// ConcurrentLinearAllocator.h and other experimental headers use this macro
#if defined(_MSC_VER) && !defined(__has_feature)
#define __has_feature(x) 0
#endif

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

extern TMap<FString,
            TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>>
    GBlueprintExistsInflight;
extern TMap<FString,
            TArray<TPair<FString, TSharedPtr<class FMcpBridgeWebSocket>>>>
    GBlueprintCreateInflight;
extern TMap<FString, double> GBlueprintCreateInflightTs;
extern FCriticalSection GBlueprintCreateMutex;
extern double GBlueprintCreateStaleTimeoutSec;
extern TSet<FString> GBlueprintBusySet;
extern TMap<FString, TSharedPtr<FJsonObject>> GBlueprintRegistry;

extern TMap<FString, TSharedPtr<FJsonObject>> GSequenceRegistry;
extern FString GCurrentSequencePath;

// Lightweight registry used for created Niagara systems when running in
// fast-mode or when native Niagara factories are not available. Tests and
// higher-level tooling may rely on a plugin-side record of created
// Niagara assets even when on-disk creation is not possible.
extern TMap<FString, TSharedPtr<FJsonObject>> GNiagaraRegistry;

extern TMap<FString, double> GRecentAssetSaveTs;
extern FCriticalSection GRecentAssetSaveMutex;
extern double GRecentAssetSaveThrottleSeconds;
