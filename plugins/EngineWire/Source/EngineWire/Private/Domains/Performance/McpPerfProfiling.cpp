#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Performance/McpPerfPrivate.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "RHI.h"
#if __has_include("RenderTimer.h")
#include "RenderTimer.h"
#else
#include "RenderCore.h"
#endif
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if __has_include("ViewportClient.h")
#include "ViewportClient.h"
#endif

#include "Editor/UnrealEd/Public/Editor.h"

namespace McpPerformanceHandlers
{
namespace
{
bool RequireEditor(
    const FPerformanceActionContext& Context,
    const FString& Message = TEXT("Editor not available"),
    const FString& ErrorCode = TEXT("NO_EDITOR"))
{
    if (GEditor)
    {
        return true;
    }
    Context.Bridge.SendAutomationError(
        Context.RequestingSocket, Context.RequestId, Message, ErrorCode);
    return false;
}

// 'stat X' only toggles, so enabled:false used to switch an overlay ON when it was off. Read the
// viewport's state (engine stats and stat groups both report through IsStatEnabled) and exec only
// when it differs, which gives set semantics.
bool SetStatOverlay(const FPerformanceActionContext& Context, const FString& Stat, bool bEnabled)
{
    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    FViewportClient* ViewportClient = ActiveViewport ? ActiveViewport->GetClient() : nullptr;
    if (!ViewportClient)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("No active editor viewport; the stat overlay state cannot be read or set. Focus a level viewport and retry."),
            TEXT("NO_VIEWPORT"));
        return true;
    }
    const bool bWasEnabled = ViewportClient->IsStatEnabled(Stat);
    if (bWasEnabled != bEnabled)
    {
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), *FString::Printf(TEXT("stat %s"), *Stat));
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("enabled"), bEnabled);
    Resp->SetBoolField(TEXT("changed"), bWasEnabled != bEnabled);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
        FString::Printf(TEXT("Stat '%s' %s"), *Stat, bEnabled ? TEXT("shown") : TEXT("hidden")), Resp);
    return true;
}
bool IsValidStatCategory(const FString& Category)
{
    for (int32 Index = 0; Index < Category.Len(); ++Index)
    {
        const TCHAR Character = Category[Index];
        if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}
}

bool HandleProfilingAction(const FPerformanceActionContext& Context)
{
    if (HandleMemoryReportAction(Context))
    {
        return true;
    }

    if (Context.Lower == TEXT("start_profiling"))
    {
        if (!RequireEditor(Context))
        {
            return true;
        }
        double Duration = 0.0;
        Context.Payload->TryGetNumberField(TEXT("duration"), Duration);
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat startfile"));
        if (Duration > 0.0)
        {
            // duration used to be ignored; stop the capture after it instead of leaving it running.
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
            {
                if (GEngine && GEditor && GEditor->GetEditorWorldContext().World())
                {
                    GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat stopfile"));
                }
                return false;
            }), static_cast<float>(Duration));
        }
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            Duration > 0.0 ? FString::Printf(TEXT("Profiling started; it stops after %.0fs"), Duration) : FString(TEXT("Profiling started; stop it with stop_profiling")), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("stop_profiling"))
    {
        if (!RequireEditor(Context))
        {
            return true;
        }

        GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat stopfile"));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Profiling stopped"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("show_fps"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        return !RequireEditor(Context) || SetStatOverlay(Context, TEXT("FPS"), bEnabled);
    }

    if (Context.Lower == TEXT("show_stats"))
    {
        FString Category;
        if (!Context.Payload->TryGetStringField(TEXT("category"), Category) ||
            Category.IsEmpty())
        {
            Context.Bridge.SendAutomationResponse(
                Context.RequestingSocket, Context.RequestId, false,
                TEXT("Category required. Pass a stat name such as unit, units, "
                     "fps, gpu, gfx, engine, game, threading, or any other "
                     "'stat <name>' console category."),
                nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        if (!RequireEditor(Context))
        {
            return true;
        }

        if (!IsValidStatCategory(Category))
        {
            Context.Bridge.SendAutomationError(
                Context.RequestingSocket, Context.RequestId,
                TEXT("Invalid stat category name. Only alphanumeric characters and underscores allowed."),
                TEXT("INVALID_CATEGORY"));
            return true;
        }

        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        return SetStatOverlay(Context, Category, bEnabled);
    }
    if (Context.Lower == TEXT("run_benchmark"))
    {
        // It answered "completed" with no measurement at all, opened the GPU Visualizer (profilegpu) and wrote a stats
        // file each run. It samples every frame now and reports the times; start_profiling writes the stats file.
        double Duration = 10.0;
        Context.Payload->TryGetNumberField(TEXT("duration"), Duration);
        const double BenchmarkDuration = FMath::Clamp(Duration, 0.5, 600.0);
        FString BenchmarkType = TEXT("all");
        Context.Payload->TryGetStringField(TEXT("type"), BenchmarkType);
        const bool bCpu = BenchmarkType.Equals(TEXT("cpu"), ESearchCase::IgnoreCase) || BenchmarkType.Equals(TEXT("all"), ESearchCase::IgnoreCase);
        const bool bGpu = BenchmarkType.Equals(TEXT("gpu"), ESearchCase::IgnoreCase) || BenchmarkType.Equals(TEXT("all"), ESearchCase::IgnoreCase);
        if (!bCpu && !bGpu)
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                FString::Printf(TEXT("type '%s' is not a benchmark type; use cpu, gpu or all."), *BenchmarkType), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!RequireEditor(Context))
        {
            return true;
        }
        const ERequestOrigin ResponseOrigin = Context.ResponseOrigin;
        Context.Bridge.SendProgressUpdate(
            Context.RequestId, 0.0f,
            FString::Printf(TEXT("Benchmark running for %.0fs"), BenchmarkDuration),
            true, ResponseOrigin);

        struct FSamples { double Elapsed = 0.0; int32 Frames = 0; double FrameMs = 0.0; double WorstMs = 0.0; double BestMs = TNumericLimits<double>::Max();
                          double GameMs = 0.0; double RenderMs = 0.0; double GpuMs = 0.0; };
        const TSharedRef<FSamples> Samples = MakeShared<FSamples>();
        TWeakObjectPtr<UEngineWireSubsystem> WeakThis(&Context.Bridge);
        const FString RequestId = Context.RequestId;
        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [WeakThis, RequestingSocket, RequestId, BenchmarkDuration, ResponseOrigin, Samples, bCpu, bGpu](float DeltaSeconds)
            {
                UEngineWireSubsystem* Subsystem = WeakThis.Get();
                if (!Subsystem)
                {
                    return false;
                }
                // The first tick closes the frame the request arrived in; it is not counted.
                if (Samples->Elapsed > 0.0)
                {
                    const double Ms = DeltaSeconds * 1000.0;
                    ++Samples->Frames;
                    Samples->FrameMs += Ms;
                    Samples->WorstMs = FMath::Max(Samples->WorstMs, Ms);
                    Samples->BestMs = FMath::Min(Samples->BestMs, Ms);
                    Samples->GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
                    Samples->RenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
                    Samples->GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
                }
                Samples->Elapsed += FMath::Max(DeltaSeconds, 1e-4f);
                if (Samples->Elapsed < BenchmarkDuration)
                {
                    return true;
                }
                const double Frames = FMath::Max(1, Samples->Frames);
                TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
                Resp->SetNumberField(TEXT("duration"), Samples->Elapsed);
                Resp->SetNumberField(TEXT("frames"), Samples->Frames);
                Resp->SetNumberField(TEXT("averageFps"), Samples->FrameMs > 0.0 ? 1000.0 * Samples->Frames / Samples->FrameMs : 0.0);
                if (bCpu)
                {
                    Resp->SetNumberField(TEXT("averageFrameMs"), Samples->FrameMs / Frames);
                    Resp->SetNumberField(TEXT("worstFrameMs"), Samples->WorstMs);
                    Resp->SetNumberField(TEXT("bestFrameMs"), Samples->Frames > 0 ? Samples->BestMs : 0.0);
                    Resp->SetNumberField(TEXT("gameThreadMs"), Samples->GameMs / Frames);
                    Resp->SetNumberField(TEXT("renderThreadMs"), Samples->RenderMs / Frames);
                }
                if (bGpu)
                {
                    Resp->SetNumberField(TEXT("gpuMs"), Samples->GpuMs / Frames);
                }
                Resp->SetStringField(TEXT("status"), TEXT("completed"));
                Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                    FString::Printf(TEXT("Benchmark completed: %d frames over %.1fs"), Samples->Frames, Samples->Elapsed),
                    Resp, FString(), ResponseOrigin);
                return false;
            }));
        return true;
    }

    if (Context.Lower == TEXT("enable_gpu_timing"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        if (IConsoleVariable* CVar =
                IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUStatsEnabled")))
        {
            CVar->Set(bEnabled ? 1 : 0);
        }

        if (bEnabled)
        {
            if (!RequireEditor(Context))
            {
                return true;
            }
            GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat gpu"));
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("enabled"), bEnabled);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("GPU timing %s"),
                            bEnabled ? TEXT("enabled") : TEXT("disabled")),
            Resp);
        return true;
    }

    return false;
}
}
