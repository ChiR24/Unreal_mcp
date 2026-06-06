#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "McpHandlerUtils.h"

#include "Containers/Ticker.h"
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
#endif

bool HandleProfilingAction(const FPerformanceActionContext& Context)
{
#if !WITH_EDITOR
    return false;
#else
    if (Context.Lower == TEXT("generate_memory_report"))
    {
        bool bDetailed = false;
        Context.Payload->TryGetBoolField(TEXT("detailed"), bDetailed);
        FString OutputPath;
        Context.Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

        if (!RequireEditor(Context))
        {
            return true;
        }

        const FString Command = bDetailed ? TEXT("memreport -full") : TEXT("memreport");
        GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Command);
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Memory report generated"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("start_profiling"))
    {
        if (!RequireEditor(Context))
        {
            return true;
        }

        GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat startfile"));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("Profiling started"), nullptr);
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

        if (!RequireEditor(Context))
        {
            return true;
        }

        GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat fps"));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            TEXT("FPS stat toggled"), nullptr);
        return true;
    }

    if (Context.Lower == TEXT("show_stats"))
    {
        FString Category;
        if (!Context.Payload->TryGetStringField(TEXT("category"), Category) ||
            Category.IsEmpty())
        {
            Context.Bridge.SendAutomationResponse(
                Context.RequestingSocket, Context.RequestId, false,
                TEXT("Category required"), nullptr, TEXT("INVALID_ARGUMENT"));
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

        GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                      *FString::Printf(TEXT("stat %s"), *Category));
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, true,
            FString::Printf(TEXT("Stat '%s' toggled"), *Category), nullptr);
        return true;
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
#endif
}
}
