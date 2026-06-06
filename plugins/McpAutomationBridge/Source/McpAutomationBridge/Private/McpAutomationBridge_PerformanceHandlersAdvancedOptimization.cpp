#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "McpHandlerUtils.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#endif

namespace McpPerformanceHandlers
{
#if WITH_EDITOR
namespace
{
void SetOptimizationCVarInt(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value);
    }
}

void SetOptimizationCVarFloat(const TCHAR* Name, float Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value);
    }
}
}
#endif

bool HandleAdvancedOptimizationAction(const FPerformanceActionContext& Context)
{
#if !WITH_EDITOR
    return false;
#else
    if (Context.Lower == TEXT("apply_baseline_settings"))
    {
        FString Profile = TEXT("balanced");
        Context.Payload->TryGetStringField(TEXT("profile"), Profile);

        if (Profile.Equals(TEXT("performance"), ESearchCase::IgnoreCase))
        {
            SetOptimizationCVarInt(TEXT("r.VSync"), 0);
            SetOptimizationCVarInt(TEXT("r.AllowHDR"), 0);
            SetOptimizationCVarInt(TEXT("r.MotionBlurQuality"), 0);
            SetOptimizationCVarInt(TEXT("r.DepthOfFieldQuality"), 0);
            SetOptimizationCVarInt(TEXT("r.BloomQuality"), 0);
            SetOptimizationCVarInt(TEXT("r.ShadowQuality"), 1);
            SetOptimizationCVarInt(TEXT("r.MaxAnisotropy"), 4);
        }
        else if (Profile.Equals(TEXT("quality"), ESearchCase::IgnoreCase))
        {
            SetOptimizationCVarInt(TEXT("r.VSync"), 1);
            SetOptimizationCVarInt(TEXT("r.AllowHDR"), 1);
            SetOptimizationCVarInt(TEXT("r.MotionBlurQuality"), 4);
            SetOptimizationCVarInt(TEXT("r.DepthOfFieldQuality"), 2);
            SetOptimizationCVarInt(TEXT("r.BloomQuality"), 5);
            SetOptimizationCVarInt(TEXT("r.ShadowQuality"), 5);
            SetOptimizationCVarInt(TEXT("r.MaxAnisotropy"), 16);
        }
        else
        {
            SetOptimizationCVarInt(TEXT("r.VSync"), 1);
            SetOptimizationCVarInt(TEXT("r.AllowHDR"), 1);
            SetOptimizationCVarInt(TEXT("r.MotionBlurQuality"), 2);
            SetOptimizationCVarInt(TEXT("r.DepthOfFieldQuality"), 1);
            SetOptimizationCVarInt(TEXT("r.BloomQuality"), 3);
            SetOptimizationCVarInt(TEXT("r.ShadowQuality"), 3);
            SetOptimizationCVarInt(TEXT("r.MaxAnisotropy"), 8);
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("profile"), Profile);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Baseline settings applied: %s"), *Profile), Resp);
        return true;
    }

    if (Context.Lower == TEXT("optimize_draw_calls"))
    {
        bool bEnabled = true;
        if (!Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled))
        {
            Context.Payload->TryGetBoolField(TEXT("enableBatching"), bEnabled);
        }

        bool bInstancing = true;
        if (!Context.Payload->TryGetBoolField(TEXT("instancing"), bInstancing))
        {
            Context.Payload->TryGetBoolField(TEXT("enableInstancing"), bInstancing);
        }

        SetOptimizationCVarInt(
            TEXT("r.MeshDrawCommands.DynamicInstancing"), bInstancing ? 1 : 0);
        SetOptimizationCVarInt(
            TEXT("r.MeshDrawCommands.UseCachedCommands"), bEnabled ? 1 : 0);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("optimized"), bEnabled);
        Resp->SetBoolField(TEXT("instancing"), bInstancing);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Draw call optimizations configured"), Resp);
        return true;
    }

    if (Context.Lower == TEXT("configure_occlusion_culling"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        double OcclusionSlop = 0.0;
        const bool bHasSlop = Context.Payload->TryGetNumberField(
            TEXT("slop"), OcclusionSlop);

        double MinScreenRadiusForOcclusion = 0.0;
        const bool bHasMinRadius = Context.Payload->TryGetNumberField(
            TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);

        SetOptimizationCVarInt(TEXT("r.AllowOcclusionQueries"), bEnabled ? 1 : 0);
        if (bHasSlop)
        {
            SetOptimizationCVarFloat(
                TEXT("r.OcclusionSlop"), static_cast<float>(OcclusionSlop));
        }
        if (bHasMinRadius)
        {
            SetOptimizationCVarFloat(
                TEXT("r.OcclusionCullMinScreenRadius"),
                static_cast<float>(MinScreenRadiusForOcclusion));
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("enabled"), bEnabled);
        if (bHasSlop)
        {
            Resp->SetNumberField(TEXT("slop"), OcclusionSlop);
        }
        if (bHasMinRadius)
        {
            Resp->SetNumberField(TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);
        }
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Occlusion culling configured"), Resp);
        return true;
    }

    if (Context.Lower == TEXT("optimize_shaders"))
    {
        FString Mode = TEXT("changed");
        Context.Payload->TryGetStringField(TEXT("mode"), Mode);

        bool bForceRecompile = false;
        Context.Payload->TryGetBoolField(TEXT("forceRecompile"), bForceRecompile);

        FString Command;
        if (bForceRecompile)
        {
            Command = TEXT("recompileshaders all");
        }
        else if (Mode.Equals(TEXT("material"), ESearchCase::IgnoreCase))
        {
            Command = TEXT("recompileshaders material");
        }
        else if (Mode.Equals(TEXT("global"), ESearchCase::IgnoreCase))
        {
            Command = TEXT("recompileshaders global");
        }
        else
        {
            Command = TEXT("recompileshaders changed");
        }

        if (!GEditor)
        {
            Context.Bridge.SendAutomationError(
                Context.RequestingSocket, Context.RequestId,
                TEXT("Editor not available"), TEXT("NO_EDITOR"));
            return true;
        }

        GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Command);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("mode"), Mode);
        Resp->SetBoolField(TEXT("forceRecompile"), bForceRecompile);
        Resp->SetStringField(TEXT("command"), Command);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Shader optimization initiated: %s"), *Command),
            Resp);
        return true;
    }

    if (Context.Lower != TEXT("configure_world_partition"))
    {
        return false;
    }

    bool bEnabled = true;
    Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double CellSize = 0.0;
    const bool bHasCellSize =
        Context.Payload->TryGetNumberField(TEXT("cellSize"), CellSize);

    double LoadingRange = 0.0;
    bool bHasLoadingRange =
        Context.Payload->TryGetNumberField(TEXT("loadingRange"), LoadingRange);
    if (!bHasLoadingRange)
    {
        bHasLoadingRange =
            Context.Payload->TryGetNumberField(TEXT("streamingDistance"), LoadingRange);
    }

    SetOptimizationCVarInt(TEXT("wp.Runtime.EnableStreaming"), bEnabled ? 1 : 0);
    if (bHasCellSize)
    {
        SetOptimizationCVarFloat(
            TEXT("wp.Runtime.RuntimeCellSize"), static_cast<float>(CellSize));
    }
    if (bHasLoadingRange)
    {
        SetOptimizationCVarFloat(
            TEXT("wp.Runtime.RuntimeStreamingRange"),
            static_cast<float>(LoadingRange));
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("streamingEnabled"), bEnabled);
    if (bHasCellSize)
    {
        Resp->SetNumberField(TEXT("cellSize"), CellSize);
    }
    if (bHasLoadingRange)
    {
        Resp->SetNumberField(TEXT("loadingRange"), LoadingRange);
    }
    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, true,
        TEXT("World Partition settings configured"), Resp);
    return true;
#endif
}
}
