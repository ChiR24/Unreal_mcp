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

// Sets the console variable Name as the console does and reads it back, recording it under Applied, or under NotSet
// with why. Its predecessor set at code priority with no read-back: a value set from the console, a read-only
// variable or one missing from this engine version all answered success.
inline void SetPerfCVar(const TCHAR* Name, const FString& Value, TArray<FString>& Applied, TArray<FString>& NotSet)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
    const FString NotTaken = CVar ? McpSetConsoleVariable(*CVar, Value) : FString(TEXT("not in this engine version"));
    if (NotTaken.IsEmpty())
    {
        Applied.Add(Name);
    }
    else
    {
        NotSet.Add(FString::Printf(TEXT("%s (%s)"), Name, *NotTaken));
    }
}

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
    TArray<TSharedPtr<FJsonValue>> AppliedJson;
    for (const FString& Name : Applied)
    {
        AppliedJson.Add(MakeShared<FJsonValueString>(Name));
    }
    TArray<TSharedPtr<FJsonValue>> NotSetJson;
    for (const FString& Entry : NotSet)
    {
        NotSetJson.Add(MakeShared<FJsonValueString>(Entry));
    }
    Resp->SetArrayField(TEXT("appliedCVars"), AppliedJson);
    Resp->SetArrayField(TEXT("notSet"), NotSetJson);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
        NotSet.Num() > 0 ? FString::Printf(TEXT("%s; not set: %s"), *Done, *FString::Join(NotSet, TEXT("; "))) : Done, Resp);
}

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
