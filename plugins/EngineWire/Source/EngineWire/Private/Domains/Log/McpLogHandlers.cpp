#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

#include "EngineWireSubsystem.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "McpConnectionManager.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Core/Subsystem/EngineWireSubsystemResponseSanitization.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/Log/McpLogHistory.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

using namespace EngineWireSubsystemResponse;

/**
 * Custom output device that captures all log output and streams it via WebSocket.
 * Thread-safe implementation using AsyncTask for game thread dispatch.
 */
class FMcpLogOutputDevice : public FOutputDevice
{
public:
    explicit FMcpLogOutputDevice(UEngineWireSubsystem* InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        // Guard against a destroyed subsystem
        if (!Subsystem.IsValid())
        {
            return;
        }

        // ---------------------------------------------------------------------
        // Filter noisy categories to prevent log spam (FName compares: no string per line)
        // ---------------------------------------------------------------------
        // Filter own logs to prevent infinite recursion
        if (Category == LogEngineWireSubsystem.GetCategoryName())
        {
            return;
        }

        // Filter highly verbose engine categories
        static const FName McpLogNoisyCategories[] = {FName(TEXT("LogRHI")), FName(TEXT("LogEOSSDK")), FName(TEXT("LogCsvProfiler"))};
        static const FName McpLogSlateStyle(TEXT("LogSlateStyle"));
        static const FName McpLogStats(TEXT("LogStats"));
        for (const FName& Noisy : McpLogNoisyCategories)
        {
            if (Category == Noisy)
            {
                return;
            }
        }

        // Filter specific noisy warnings
        if (Verbosity == ELogVerbosity::Warning && Category == McpLogSlateStyle)
        {
            // "Missing Resource from 'ProfileVisualizerStyle'" is known engine warning
            if (FString(V).Contains(TEXT("Missing Resource from 'ProfileVisualizerStyle'")))
            {
                return;
            }
        }

        // Filter "no thread with id" noise from stat commands
        if (Category == McpLogStats && FString(V).Contains(TEXT("There is no thread with id")))
        {
            return;
        }

        const FString CategoryStr = Category.ToString();
        const FString VerbosityString =
            ToString(static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask));

        // Runs inline on whichever thread logged, which can be the one holding the engine's mount lock (a failed
        // dismount logs under it), so it never asks the engine which content mounts are registered.
        const FString Message =
            SanitizeEngineErrorForResponse(FString(V), /*bNameMounts=*/false).Left(2048);
        AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem = Subsystem, CategoryStr, VerbosityString, Message]()
        {
            if (UEngineWireSubsystem* StrongSubsystem = WeakSubsystem.Get())
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
    TWeakObjectPtr<UEngineWireSubsystem> Subsystem;
};

void UEngineWireSubsystem::ReconcileLogCaptureDevice()
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

bool UEngineWireSubsystem::HandleLogAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Its one caller, system_control's dispatcher, has refused a missing payload already.
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    auto SendLogSubscriptionResponse = [this, RequestingSocket, &RequestId](
        const FString& ResponseAction,
        const bool bSubscribed,
        const bool bWasAlreadySubscribed,
        const FString& Message) -> void
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
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
        double RequestedLines = GetJsonNumberField(Payload, TEXT("lines"), 100.0);
        const int32 MaxLines = FMath::Clamp(static_cast<int32>(RequestedLines), 1, 1000);
        const ELogVerbosity::Type ParsedVerbosity =
            ParseLogVerbosityFromString(GetJsonStringField(Payload, TEXT("minVerbosity")));
        // Missing/unknown -> Log; "verbose" returns everything (VeryVerbose too), as the record documents.
        const ELogVerbosity::Type MinVerbosity =
            ParsedVerbosity == ELogVerbosity::NoLogging ? ELogVerbosity::Log
            : ParsedVerbosity == ELogVerbosity::Verbose ? ELogVerbosity::VeryVerbose
            : ParsedVerbosity;

        int32 Matched = 0;
        // Live Coding tells the editor log only "failed, please see Live
        // console". The console's own log covers patching; the compiler errors
        // behind a failure are in UnrealBuildTool's log, since UBT is what the
        // console runs to compile. UBT picks that folder exactly this way.
        const FString Source = GetJsonStringField(Payload, TEXT("source")).ToLower();
        double RunsBack = GetJsonNumberField(Payload, TEXT("runsBack"), 1.0);
        const int32 Run = FMath::Clamp(static_cast<int32>(RunsBack), 1, 20);
        const FString FilePath = Source == TEXT("livecoding") ? FMcpLogHistory::LiveCodingLogPath()
            : Source == TEXT("previous") ? FMcpLogHistory::PreviousRunLogPath(Run)
            : Source != TEXT("build") ? FString()
            : FApp::IsEngineInstalled()
            ? FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("UnrealBuildTool/Log.txt"))
            : FPaths::Combine(FPaths::EngineDir(), TEXT("Programs/UnrealBuildTool/Log.txt"));
        // A log file has no parsed category or verbosity to filter on; they used to be dropped silently.
        if (!Source.IsEmpty() && Source != TEXT("editor") &&
            (!GetJsonStringField(Payload, TEXT("category")).IsEmpty() || !GetJsonStringField(Payload, TEXT("minVerbosity")).IsEmpty()))
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("category and minVerbosity only filter the editor log; with source %s use filter (text, | for alternatives) instead."), *Source),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (Source == TEXT("previous") && FilePath.IsEmpty())
        {
            // Falling through would answer with THIS run's log instead.
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No log from %d editor run(s) back was found."), Run), TEXT("NOT_FOUND"));
            return true;
        }
        // Every line is searched once per alternative: a few is what a reader types.
        constexpr int32 MaxLogFilterAlternatives = 16;
        const FString Filter = GetJsonStringField(Payload, TEXT("filter"));
        TArray<FString> Alternatives;
        if (Filter.ParseIntoArray(Alternatives, TEXT("|"), true) > MaxLogFilterAlternatives || Filter.Len() > 1024)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("filter takes at most %d alternatives and 1024 characters."), MaxLogFilterAlternatives),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        // Lines come back as a reply shows them (redacted), and the filter is matched against that form.
        const TArray<FString> Lines = !FilePath.IsEmpty()
            ? FMcpLogHistory::ReadFileTail(FilePath, MaxLines, Filter, Matched)
            : FMcpLogHistory::Get().Read(MaxLines, Filter, GetJsonStringField(Payload, TEXT("category")), MinVerbosity,
                                         Matched);
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetArrayField(TEXT("lines"), McpHandlerUtils::ToJsonStringArray(Lines));
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
