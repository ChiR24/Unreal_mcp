#pragma once

#include "McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class AActor;
class UPrimitiveComponent;
class UWorld;

namespace McpPerformanceHandlers
{
struct FPerformanceActionContext
{
    UMcpAutomationBridgeSubsystem& Bridge;
    const FString& RequestId;
    const TSharedPtr<FJsonObject>& Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    const FString& Lower;
    ERequestOrigin ResponseOrigin;
};

FString ResolvePerformanceAction(
    const FString& RequestAction,
    const TSharedPtr<FJsonObject>& Payload);
bool IsPerformanceAction(const FString& RequestAction, const FString& Lower);
bool HandleProfilingAction(const FPerformanceActionContext& Context);
bool HandleRenderingSettingsAction(const FPerformanceActionContext& Context);
bool HandleActorMergeAction(const FPerformanceActionContext& Context);
bool HandleAdvancedOptimizationAction(const FPerformanceActionContext& Context);
AActor* ResolveMergeActorByName(UWorld* World, const FString& Name);
void CollectMergeComponents(
    const TArray<AActor*>& ActorsToMerge,
    TArray<UPrimitiveComponent*>& ComponentsToMerge);
}
