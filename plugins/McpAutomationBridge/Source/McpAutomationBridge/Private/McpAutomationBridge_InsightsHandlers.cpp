// =============================================================================
// McpAutomationBridge_InsightsHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Profiling & Insights Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_insights
//   - start_session: Start Unreal Insights trace session with optional channels
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: Trace system (built-in)
// 
// Notes:
//   - Uses console command "Trace.Start [channels]" for compatibility
//   - Channels are optional; default trace starts without specific channels
//   - Trace data sent to Unreal Insights application
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

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleInsightsAction(
    const FString& RequestId, 
    const FString& Action, 
    const TSharedPtr<FJsonObject>& Payload, 
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_insights"))
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
    // start_session: Start trace session with optional channels
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("start_session"))
    {
        FString Channels;
        const bool bHasChannels = Payload->TryGetStringField(TEXT("channels"), Channels) 
            && !Channels.IsEmpty();

        // Execute trace start via console command
        // This is the standard way to control trace from editor
        if (bHasChannels)
        {
            GEngine->Exec(nullptr, *FString::Printf(TEXT("Trace.Start %s"), *Channels));
        }
        else
        {
            GEngine->Exec(nullptr, TEXT("Trace.Start"));
        }

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("start_trace"));
        Result->SetStringField(TEXT("status"), TEXT("started"));
        if (bHasChannels)
        {
            Result->SetStringField(TEXT("channels"), Channels);
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Trace session started."), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
