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

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "Misc/OutputDevice.h"
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

        // ---------------------------------------------------------------------
        // Convert verbosity to string
        // ---------------------------------------------------------------------
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
    // Validate action
    if (Action != TEXT("manage_logs"))
    {
        return false;
    }

    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Extract subaction
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

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
