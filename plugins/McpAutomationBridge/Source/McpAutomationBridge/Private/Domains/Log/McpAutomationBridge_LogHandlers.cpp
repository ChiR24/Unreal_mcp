// =============================================================================
// McpAutomationBridge_LogHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Log Streaming Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_logs
//   - subscribe: Enable log streaming to connected clients
//   - unsubscribe: Disable log streaming
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: OutputDevice, Async
//
// Architecture:
//   - FMcpLogOutputDevice: Custom FOutputDevice that intercepts all log output
//   - Thread-safe: Uses AsyncTask to dispatch to game thread for socket sending
//   - Filtering: Excludes noisy categories (LogRHI, LogEOSSDK, LogCsvProfiler)
//
// Notes:
//   - LogCaptureDevice lifetime managed by subsystem
//   - Weak pointer used to prevent crashes if subsystem destroyed during callback
// =============================================================================

#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "McpConnectionManager.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/Log/McpAutomationBridge_LogHistory.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

using namespace McpAutomationBridgeSubsystemResponse;

// =============================================================================
// FMcpLogOutputDevice - Custom Log Capture Device
// =============================================================================

/**
 * Custom output device that captures all log output and streams it via WebSocket.
 * Thread-safe implementation using AsyncTask for game thread dispatch.
 */
class FMcpLogOutputDevice : public FOutputDevice
{
public:
    explicit FMcpLogOutputDevice(UMcpAutomationBridgeSubsystem* InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        // Guard against null or destroyed subsystem
        if (!Subsystem || !Subsystem->IsValidLowLevel())
        {
            return;
        }

        // ---------------------------------------------------------------------
        // Filter noisy categories to prevent log spam
        // ---------------------------------------------------------------------
        const FString CategoryStr = Category.ToString();

        // Filter own logs to prevent infinite recursion
        if (Category == LogMcpAutomationBridgeSubsystem.GetCategoryName())
        {
            return;
        }

        // Filter highly verbose engine categories
        static const TArray<FString> NoisyCategories = {
            TEXT("LogRHI"),
            TEXT("LogEOSSDK"),
            TEXT("LogCsvProfiler")
        };

        if (NoisyCategories.Contains(CategoryStr))
        {
            return;
        }

        // Filter specific noisy warnings
        if (Verbosity == ELogVerbosity::Warning && CategoryStr == TEXT("LogSlateStyle"))
        {
            // "Missing Resource from 'ProfileVisualizerStyle'" is known engine warning
            if (FString(V).Contains(TEXT("Missing Resource from 'ProfileVisualizerStyle'")))
            {
                return;
            }
        }

        // Filter "no thread with id" noise from stat commands
        if (CategoryStr == TEXT("LogStats") && FString(V).Contains(TEXT("There is no thread with id")))
        {
            return;
        }

        FString VerbosityString;
        switch (Verbosity)
        {
            case ELogVerbosity::Fatal:       VerbosityString = TEXT("Fatal");       break;
            case ELogVerbosity::Error:       VerbosityString = TEXT("Error");       break;
            case ELogVerbosity::Warning:     VerbosityString = TEXT("Warning");     break;
            case ELogVerbosity::Display:     VerbosityString = TEXT("Display");     break;
            case ELogVerbosity::Log:         VerbosityString = TEXT("Log");         break;
            case ELogVerbosity::Verbose:     VerbosityString = TEXT("Verbose");     break;
            case ELogVerbosity::VeryVerbose: VerbosityString = TEXT("VeryVerbose"); break;
            default:                         VerbosityString = TEXT("Log");         break;
        }

        const FString Message =
            SanitizeEngineErrorForResponse(FString(V)).Left(2048);
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);

        AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, CategoryStr, VerbosityString, Message]()
        {
            if (UMcpAutomationBridgeSubsystem* StrongSubsystem = WeakSubsystem.Get())
            {
                TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
                Payload->SetStringField(TEXT("category"), CategoryStr);
                Payload->SetStringField(TEXT("verbosity"), VerbosityString);
                Payload->SetStringField(TEXT("message"), Message);

                TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
                Event->SetStringField(TEXT("type"), TEXT("automation_event"));
                Event->SetStringField(TEXT("event"), TEXT("log"));
                Event->SetObjectField(TEXT("payload"), Payload);

                StrongSubsystem->BroadcastAutomationEvent(Event);
            }
        });
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem;
};

// =============================================================================
// Handler Implementation
// =============================================================================

void UMcpAutomationBridgeSubsystem::ReconcileLogCaptureDevice()
{
    if (!LogCaptureDevice.IsValid())
    {
        return;
    }

    const bool bHasNativeSubscribers =
        NativeTransport && NativeTransport->HasLogEventSubscribers();
    const bool bHasWebSocketSubscribers =
        ConnectionManager.IsValid() && ConnectionManager->HasLogSubscribers();
    if (bHasNativeSubscribers || bHasWebSocketSubscribers)
    {
        return;
    }

    if (GLog)
    {
        GLog->RemoveOutputDevice(LogCaptureDevice.Get());
    }
    LogCaptureDevice.Reset();
}

bool UMcpAutomationBridgeSubsystem::HandleLogAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_logs"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    auto SendLogSubscriptionResponse = [this, RequestingSocket, &RequestId](
        const FString& ResponseAction,
        const bool bSubscribed,
        const bool bWasAlreadySubscribed,
        const FString& Message) -> void
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("action"), TEXT("manage_logs"));
        Result->SetStringField(TEXT("subAction"), ResponseAction);
        Result->SetBoolField(TEXT("subscribed"), bSubscribed);
        if (ResponseAction == TEXT("subscribe"))
        {
            Result->SetBoolField(TEXT("wasAlreadySubscribed"), bWasAlreadySubscribed);
        }

        SendAutomationResponse(
            RequestingSocket,
            RequestId,
            true,
            Message,
            Result,
            FString());
    };

    // -------------------------------------------------------------------------
    // subscribe: Enable log streaming
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("subscribe"))
    {
        const bool bWasAlreadySubscribed = LogCaptureDevice.IsValid();
        if (NativeTransport)
        {
            NativeTransport->SetLogEventSubscriptionForRequest(
                RequestId, true);
        }
        if (ConnectionManager.IsValid() && RequestingSocket.IsValid())
        {
            ConnectionManager->SetLogSubscription(RequestingSocket, true);
        }
        SendLogSubscriptionResponse(TEXT("subscribe"), true, bWasAlreadySubscribed,
            TEXT("Subscribed to editor logs."));

        if (!LogCaptureDevice.IsValid())
        {
            // Register only after the request response is sent. Adding the output
            // device first can capture response-side logging and starve the
            // waiting bridge client under high log volume.
            LogCaptureDevice = MakeShared<FMcpLogOutputDevice>(this);
            GLog->AddOutputDevice(LogCaptureDevice.Get());
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // unsubscribe: Disable log streaming
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("unsubscribe"))
    {
        if (NativeTransport)
        {
            NativeTransport->SetLogEventSubscriptionForRequest(
                RequestId, false);
        }
        if (ConnectionManager.IsValid() && RequestingSocket.IsValid())
        {
            ConnectionManager->SetLogSubscription(RequestingSocket, false);
        }
        SendLogSubscriptionResponse(TEXT("unsubscribe"), false, LogCaptureDevice.IsValid(),
            TEXT("Unsubscribed from editor logs."));

        ReconcileLogCaptureDevice();
        return true;
    }

    // -------------------------------------------------------------------------
    // read_log: the recent log history, no subscription needed
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("read_log"))
    {
        double RequestedLines = 100.0;
        Payload->TryGetNumberField(TEXT("lines"), RequestedLines);
        const int32 MaxLines = FMath::Clamp(static_cast<int32>(RequestedLines), 1, 1000);
        const FString MinText = GetJsonStringField(Payload, TEXT("minVerbosity")).ToLower();
        const ELogVerbosity::Type MinVerbosity =
            MinText == TEXT("error") ? ELogVerbosity::Error
            : MinText == TEXT("warning") ? ELogVerbosity::Warning
            : MinText == TEXT("display") ? ELogVerbosity::Display
            : MinText == TEXT("verbose") ? ELogVerbosity::VeryVerbose
            : ELogVerbosity::Log;

        int32 Matched = 0;
        // Live Coding tells the editor log only "failed, please see Live
        // console". The console's own log covers patching; the compiler errors
        // behind a failure are in UnrealBuildTool's log, since UBT is what the
        // console runs to compile. UBT picks that folder exactly this way.
        const FString Source = GetJsonStringField(Payload, TEXT("source")).ToLower();
        double RunsBack = 1.0;
        Payload->TryGetNumberField(TEXT("runsBack"), RunsBack);
        const int32 Run = FMath::Clamp(static_cast<int32>(RunsBack), 1, 20);
        const FString FilePath = Source == TEXT("livecoding")
            ? FPaths::Combine(FPaths::EngineDir(), TEXT("Programs/LiveCodingConsole/Saved/Logs/LiveCodingConsole.log"))
            : Source == TEXT("previous") ? FMcpLogHistory::PreviousRunLogPath(Run)
            : Source != TEXT("build") ? FString()
            : FApp::IsEngineInstalled()
            ? FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("UnrealBuildTool/Log.txt"))
            : FPaths::Combine(FPaths::EngineDir(), TEXT("Programs/UnrealBuildTool/Log.txt"));
        if (Source == TEXT("previous") && FilePath.IsEmpty())
        {
            // Falling through would answer with THIS run's log instead.
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No log from %d editor run(s) back was found."), Run), TEXT("NOT_FOUND"));
            return true;
        }
        const TArray<FString> Lines = !FilePath.IsEmpty()
            ? FMcpLogHistory::ReadFileTail(FilePath, MaxLines, GetJsonStringField(Payload, TEXT("filter")), Matched)
            : FMcpLogHistory::Get().Read(
                  MaxLines, GetJsonStringField(Payload, TEXT("filter")),
                  GetJsonStringField(Payload, TEXT("category")), MinVerbosity, Matched);
        TArray<TSharedPtr<FJsonValue>> LineValues;
        for (const FString& Line : Lines)
        {
            LineValues.Add(MakeShared<FJsonValueString>(
                SanitizeEngineErrorForResponse(FMcpLogHistory::KeepDiagnosticFileName(Line))));
        }
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("lines"), LineValues);
        Result->SetNumberField(TEXT("returned"), Lines.Num());
        Result->SetNumberField(TEXT("matched"), Matched);
        if (!FilePath.IsEmpty())
        {
            // Which run a "previous" read landed on: the name carries its start time.
            Result->SetStringField(TEXT("logFile"), FPaths::GetCleanFilename(FilePath));
        }
        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Read %d of %d matching log line(s)."), Lines.Num(), Matched),
            Result, FString());
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
