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

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"
#include "Misc/OutputDevice.h"
#include "Async/Async.h"

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

        // ---------------------------------------------------------------------
        // Build JSON payload and dispatch to game thread
        // ---------------------------------------------------------------------
        const FString Message = FString(V);
        const FString CategoryString = Category.ToString();

        // Build escaped JSON payload
        const FString PayloadJson = FString::Printf(
            TEXT("{\"event\":\"log\",\"category\":\"%s\",\"verbosity\":\"%s\",\"message\":\"%s\"}"),
            *CategoryString,
            *VerbosityString,
            *Message.ReplaceCharWithEscapedChar()
        );

        // Use weak pointer to prevent crash if subsystem destroyed during async
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);

        // Dispatch to game thread for safe socket access
        AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, PayloadJson]()
        {
            if (UMcpAutomationBridgeSubsystem* StrongSubsystem = WeakSubsystem.Get())
            {
                StrongSubsystem->SendRawMessage(PayloadJson);
            }
        });
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem;
};

// =============================================================================
// Handler Implementation
// =============================================================================

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

    // -------------------------------------------------------------------------
    // subscribe: Enable log streaming
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("subscribe"))
    {
        if (!LogCaptureDevice.IsValid())
        {
            // Create and register log capture device
            LogCaptureDevice = MakeShared<FMcpLogOutputDevice>(this);
            GLog->AddOutputDevice(LogCaptureDevice.Get());
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display, 
                TEXT("Log streaming enabled by client request."));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("subscribe"));
        Result->SetBoolField(TEXT("subscribed"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Subscribed to editor logs."), Result);
        return true;
    }

    // -------------------------------------------------------------------------
    // unsubscribe: Disable log streaming
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("unsubscribe"))
    {
        if (LogCaptureDevice.IsValid())
        {
            // Remove and destroy log capture device
            GLog->RemoveOutputDevice(LogCaptureDevice.Get());
            LogCaptureDevice.Reset();
            UE_LOG(LogMcpAutomationBridgeSubsystem, Display, 
                TEXT("Log streaming disabled by client request."));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("unsubscribe"));
        Result->SetBoolField(TEXT("subscribed"), false);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Unsubscribed from editor logs."), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
