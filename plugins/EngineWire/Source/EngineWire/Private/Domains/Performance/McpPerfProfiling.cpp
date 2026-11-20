#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Performance/McpPerfPrivate.h"
#include "Foundation/Render/McpConsoleVariableWrite.h"
#include "Foundation/Render/McpViewportStats.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "RHI.h"
#if __has_include("RenderTimer.h")
#include "RenderTimer.h"
#else
#include "RenderCore.h"
#endif
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#if __has_include("ViewportClient.h")
#include "ViewportClient.h"
#endif

#include "Editor/UnrealEd/Public/Editor.h"

namespace McpPerformanceHandlers
{
namespace
{
// 'stat X' only toggles, so enabled:false used to switch an overlay ON when it was off. Read the
// viewport's state (engine stats and stat groups both report through IsStatEnabled) and exec only
// when it differs, which gives set semantics.
bool SetStatOverlay(const FPerformanceActionContext& Context, const FString& Stat, bool bEnabled)
{
    // Read where a Stat command lands, the running game's viewport during Play: the level viewport read here was the
    // wrong one then, so a second show hid the overlay and a hide could show it.
    const bool bWasEnabled = McpIsStatShown(Stat);
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

    if (Context.Lower == TEXT("read_profile"))
    {
        return HandleReadCsvProfile(Context);
    }
    // format csv runs the CSV profiler (a plain table per frame under Saved/Profiling/CSV, which read_profile summarizes)
    // instead of the stats file nothing here reads.
    FString Format;
    Context.Payload->TryGetStringField(TEXT("format"), Format);
    const bool bCsv = Format.Equals(TEXT("csv"), ESearchCase::IgnoreCase);
    const TCHAR* const StopCommand = bCsv ? TEXT("csvprofile stop") : TEXT("stat stopfile");
    if (Context.Lower == TEXT("start_profiling"))
    {
        double Duration = 0.0;
        Context.Payload->TryGetNumberField(TEXT("duration"), Duration);
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), bCsv ? TEXT("csvprofile start") : TEXT("stat startfile"));
        if (Duration > 0.0)
        {
            // duration used to be ignored; stop the capture after it instead of leaving it running.
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([StopCommand](float)
            {
                if (GEngine && GEditor && GEditor->GetEditorWorldContext().World())
                {
                    GEngine->Exec(GEditor->GetEditorWorldContext().World(), StopCommand);
                }
                return false;
            }), static_cast<float>(Duration));
        }
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            Duration > 0.0 ? FString::Printf(TEXT("Profiling started; it stops after %.0fs"), Duration)
                           : FString(bCsv ? TEXT("CSV profile started; stop it with stop_profiling format csv") : TEXT("Profiling started; stop it with stop_profiling")), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("stop_profiling"))
    {
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), StopCommand);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            bCsv ? TEXT("CSV profile stopped; read_profile summarizes it once the file is written") : TEXT("Profiling stopped"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("show_fps"))
    {
        bool bEnabled = true;
        Context.Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
        return SetStatOverlay(Context, TEXT("FPS"), bEnabled);
    }

    if (Context.Lower == TEXT("show_stats"))
    {
        FString Category;
        if (!Context.Payload->TryGetStringField(TEXT("category"), Category) ||
            Category.IsEmpty())
        {
            Context.Bridge.SendAutomationError(
                Context.RequestingSocket, Context.RequestId,
                TEXT("Category required. Pass a stat name such as unit, units, "
                     "fps, gpu, gfx, engine, game, threading, or any other "
                     "'stat <name>' console category."),
                TEXT("INVALID_ARGUMENT"));
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
                    // GPU timing off reads 0, which fit_frame_budget refuses; a benchmark still answers, and says so.
                    if (Samples->GpuMs <= 0.0)
                    {
                        Resp->SetArrayField(TEXT("warnings"), McpHandlerUtils::ToJsonStringArray({FString(TEXT(
                            "The GPU's frame time reads 0 here, so gpuMs measured nothing: turn GPU timing on "
                            "(system_control enable_gpu_timing) and call again."))}));
                    }
                }
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

        // Set as the console does and read back; a code-priority Set lost to any console set without a word. The variable
        // gates GPU timing up to UE 5.7; UE 5.8 times the GPU always and has none.
        IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUStatsEnabled"));
        const FString NotTaken = CVar ? McpSetConsoleVariable(*CVar, bEnabled ? TEXT("1") : TEXT("0")) : FString();
        if (!NotTaken.IsEmpty())
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
                FString::Printf(TEXT("r.GPUStatsEnabled %s; GPU timing was not changed."), *NotTaken), TEXT("CVAR_UNAVAILABLE"));
            return true;
        }
        // "stat gpu" toggles: run while the overlay was already up it hid it, and enabled false never hid it.
        if (McpIsStatShown(TEXT("GPU")) != bEnabled)
        {
            GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat gpu"));
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("enabled"), bEnabled);
        Resp->SetBoolField(TEXT("overlayShown"), McpIsStatShown(TEXT("GPU")));
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
