#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Performance/McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "ViewportClient.h"

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
        double Duration = 60.0;
        Context.Payload->TryGetNumberField(TEXT("duration"), Duration);
        const double BenchmarkDuration = FMath::Max(0.0, Duration);

        FString BenchmarkType = TEXT("all");
        Context.Payload->TryGetStringField(TEXT("type"), BenchmarkType);

        if (!RequireEditor(Context))
        {
            return true;
        }

        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (!GEngine || !World)
        {
            Context.Bridge.SendAutomationError(
                Context.RequestingSocket, Context.RequestId,
                TEXT("Editor world not available"), TEXT("NO_WORLD"));
            return true;
        }

        const ERequestOrigin ResponseOrigin = Context.ResponseOrigin;
        GEngine->Exec(World, TEXT("stat startfile"));

        if (BenchmarkType.Equals(TEXT("gpu"), ESearchCase::IgnoreCase) ||
            BenchmarkType.Equals(TEXT("all"), ESearchCase::IgnoreCase))
        {
            GEngine->Exec(World, TEXT("profilegpu"));
        }

        Context.Bridge.SendProgressUpdate(
            Context.RequestId, 0.0f,
            FString::Printf(TEXT("Benchmark running for %.0fs"), BenchmarkDuration),
            true, ResponseOrigin);

        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(&Context.Bridge);
        const FString RequestId = Context.RequestId;
        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [WeakThis, RequestingSocket, RequestId, BenchmarkType,
                 BenchmarkDuration, ResponseOrigin](float)
                {
                    UMcpAutomationBridgeSubsystem* Subsystem = WeakThis.Get();
                    if (!Subsystem)
                    {
                        return false;
                    }

                    if (!GEditor || !GEngine)
                    {
                        Subsystem->SendAutomationResponse(
                            RequestingSocket, RequestId, false,
                            TEXT("Editor not available while completing benchmark"),
                            nullptr, TEXT("NO_EDITOR"), ResponseOrigin);
                        return false;
                    }

                    UWorld* StopWorld = GEditor->GetEditorWorldContext().World();
                    if (!StopWorld)
                    {
                        Subsystem->SendAutomationResponse(
                            RequestingSocket, RequestId, false,
                            TEXT("Editor world not available while completing benchmark"),
                            nullptr, TEXT("NO_WORLD"), ResponseOrigin);
                        return false;
                    }

                    GEngine->Exec(StopWorld, TEXT("stat stopfile"));
                    GEngine->Exec(StopWorld, TEXT("stat none"));

                    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
                    Resp->SetNumberField(TEXT("duration"), BenchmarkDuration);
                    Resp->SetStringField(TEXT("type"), BenchmarkType);
                    Resp->SetStringField(TEXT("status"), TEXT("completed"));

                    Subsystem->SendAutomationResponse(
                        RequestingSocket, RequestId, true,
                        FString::Printf(
                            TEXT("Benchmark completed (type: %s, duration: %.0fs)"),
                            *BenchmarkType, BenchmarkDuration),
                        Resp, FString(), ResponseOrigin);
                    return false;
                }),
            static_cast<float>(BenchmarkDuration));
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
