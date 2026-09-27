#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Templates/SharedPointer.h"

class AActor;
class UPrimitiveComponent;
class UWorld;

namespace McpPerformanceHandlers
{
// Sets the console variable Name when this engine has it (int32, float, bool or text Value).
template <typename T>
void SetCVarIfExists(const TCHAR* Name, T Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value);
    }
}

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
bool HandleMemoryReportAction(const FPerformanceActionContext& Context);
bool HandleProfilingAction(const FPerformanceActionContext& Context);
bool HandleRenderingSettingsAction(const FPerformanceActionContext& Context);
bool HandleActorMergeAction(const FPerformanceActionContext& Context);
bool HandleAdvancedOptimizationAction(const FPerformanceActionContext& Context);
AActor* ResolveMergeActorByName(UWorld* World, const FString& Name);
void CollectMergeComponents(
    const TArray<AActor*>& ActorsToMerge,
    TArray<UPrimitiveComponent*>& ComponentsToMerge);
}
