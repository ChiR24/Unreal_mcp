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
//   - run_tests: Execute automation tests by filter via FAutomationTestFramework
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: AutomationTest module
// 
// Notes:
//   - Tests run asynchronously; results appear in logs
//   - StartTestByName() initiates test execution
//   - For synchronous results, would require OnTestEnd delegate binding
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
#include "Misc/AutomationTest.h"

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleTestAction(
    const FString& RequestId, 
    const FString& Action, 
    const TSharedPtr<FJsonObject>& Payload, 
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_tests"))
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
    // run_tests: Execute automation tests by filter
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("run_tests"))
    {
        FString Filter;
        Payload->TryGetStringField(TEXT("filter"), Filter);

        // Note: FAutomationTestFramework::StartTestByName() runs asynchronously
        // Results are delivered via OnTestEnd delegate (global)
        // For this bridge, we confirm test initiation; check logs for results
        
        FAutomationTestFramework::Get().StartTestByName(Filter, 0);

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("action"), TEXT("run_tests"));
        Result->SetStringField(TEXT("filter"), Filter);
        Result->SetBoolField(TEXT("started"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Tests started. Check logs for results."), Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
