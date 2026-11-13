#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Render/McpConsoleVariableWrite.h"

#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Templates/SharedPointer.h"

class AActor;
class UPrimitiveComponent;
class UWorld;

namespace McpPerformanceHandlers
{
struct FPerformanceActionContext
{
    UEngineWireSubsystem& Bridge;
    const FString& RequestId;
    const TSharedPtr<FJsonObject>& Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    const FString& Lower;
    ERequestOrigin ResponseOrigin;
};

// The reply of a console-driven action: appliedCVars and notSet, refused with CVAR_UNAVAILABLE when none took.
inline void SendPerfCVarResult(const FPerformanceActionContext& Context, const FString& Done, const TArray<FString>& Applied,
                               const TArray<FString>& NotSet, TSharedPtr<FJsonObject> Resp = nullptr)
{
    if (Applied.Num() == 0 && NotSet.Num() > 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("Nothing took: %s. "
            "Nothing was changed."), *FString::Join(NotSet, TEXT("; "))), TEXT("CVAR_UNAVAILABLE"));
        return;
    }
    if (!Resp.IsValid())
    {
        Resp = McpHandlerUtils::CreateResultObject();
    }
    TArray<TSharedPtr<FJsonValue>> AppliedJson = McpHandlerUtils::ToJsonStringArray(Applied);
    TArray<TSharedPtr<FJsonValue>> NotSetJson = McpHandlerUtils::ToJsonStringArray(NotSet);
    Resp->SetArrayField(TEXT("appliedCVars"), AppliedJson);
    Resp->SetArrayField(TEXT("notSet"), NotSetJson);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
        NotSet.Num() > 0 ? FString::Printf(TEXT("%s; not set: %s"), *Done, *FString::Join(NotSet, TEXT("; "))) : Done, Resp);
}

bool HandleMemoryReportAction(const FPerformanceActionContext& Context);
bool HandleProfilingAction(const FPerformanceActionContext& Context);
bool HandleRenderingSettingsAction(const FPerformanceActionContext& Context);
bool HandleActorMergeAction(const FPerformanceActionContext& Context);
bool HandleAdvancedOptimizationAction(const FPerformanceActionContext& Context);
void CollectMergeComponents(
    const TArray<AActor*>& ActorsToMerge,
    TArray<UPrimitiveComponent*>& ComponentsToMerge);
}
