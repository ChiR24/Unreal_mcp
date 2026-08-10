// =============================================================================
// McpAutomationBridge_TestHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Test Automation Handlers
//
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
//
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_tests
//   - run_tests: Execute automation tests by filter via editor automation command
//
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem
//
// Notes:
//   - Tests run asynchronously; results appear in logs/automation_event streams
// =============================================================================

#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h" // UE5.8: explicit include (unity regroup no longer pulls GetJsonStringField transitively)

#include "Dom/JsonObject.h"

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleTestAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_tests"))
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

    // -------------------------------------------------------------------------
    // run_tests: Execute automation tests by filter
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("run_tests"))
    {
        return McpSystemControlHandlers::HandleRunTests(
            this, RequestId, Payload, RequestingSocket);
    }

    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
