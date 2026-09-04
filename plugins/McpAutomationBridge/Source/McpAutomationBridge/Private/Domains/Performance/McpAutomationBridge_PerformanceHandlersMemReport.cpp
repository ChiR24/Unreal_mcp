#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Performance/McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace McpPerformanceHandlers
{
#if WITH_EDITOR
namespace
{
FString FindNewestMemReport(const FString& ReportDirectory, FDateTime& OutStamp)
{
    TArray<FString> ReportFiles;
    IFileManager::Get().FindFilesRecursive(
        ReportFiles, *ReportDirectory, TEXT("*.memreport"), true, false, true);
    FString Newest;
    OutStamp = FDateTime::MinValue();
    for (const FString& File : ReportFiles)
    {
        const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*File);
        if (Stamp > OutStamp)
        {
            OutStamp = Stamp;
            Newest = File;
        }
    }
    return Newest;
}
}
#endif

// generate_memory_report: runs memreport and answers with the file it produced.
// The engine writes the report at the end of the frame, so the response is
// deferred through a ticker instead of blocking the game thread (which could
// never observe the new file, dogfood #172).
bool HandleMemoryReportAction(const FPerformanceActionContext& Context)
{
#if !WITH_EDITOR
    return false;
#else
    if (Context.Lower != TEXT("generate_memory_report"))
    {
        return false;
    }
    bool bDetailed = false;
    Context.Payload->TryGetBoolField(TEXT("detailed"), bDetailed);
    if (!GEditor)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    const FString Command = bDetailed ? TEXT("memreport -full") : TEXT("memreport");
    const FString ReportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir() / TEXT("MemReports"));
    FDateTime BeforeStamp;
    const FString BeforeReport = FindNewestMemReport(ReportDirectory, BeforeStamp);
    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Command);

    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakBridge = &Context.Bridge;
    const FString RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> Socket = Context.RequestingSocket;
    TSharedPtr<int32> Attempts = MakeShared<int32>(0);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [WeakBridge, RequestId, Socket, ReportDirectory, BeforeReport, Attempts](float) -> bool
        {
            FDateTime NewestStamp;
            FString NewestReport = FindNewestMemReport(ReportDirectory, NewestStamp);
            const bool bIsNew = !NewestReport.IsEmpty() && NewestReport != BeforeReport;
            ++(*Attempts);
            if (!bIsNew && *Attempts < 100)
            {
                return true; // keep polling (~10 s at 0.1 s)
            }
            if (UMcpAutomationBridgeSubsystem* Bridge = WeakBridge.Get())
            {
                TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
                Resp->SetStringField(TEXT("reportDirectory"), ReportDirectory);
                Resp->SetBoolField(TEXT("reportIsNew"), bIsNew);
                if (!NewestReport.IsEmpty())
                {
                    Resp->SetStringField(TEXT("reportPath"), FPaths::ConvertRelativePathToFull(NewestReport));
                }
                Bridge->SendAutomationResponse(Socket, RequestId, true,
                    bIsNew ? TEXT("Memory report generated") : TEXT("memreport ran but no new report file appeared within 10 s; reportPath is the newest existing report"),
                    Resp);
            }
            return false;
        }), 0.1f);
    return true;
#endif
}
}
