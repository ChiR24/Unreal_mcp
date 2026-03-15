// =============================================================================
// McpAutomationBridge_DebugHandlers.cpp
// =============================================================================
// MCP Automation Bridge - Gameplay Debugger Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_debug
//   - spawn_category: Toggle gameplay debugger category visibility
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: GameplayDebugger module (optional)
// 
// Notes:
//   - Uses console command "GameplayDebuggerCategory [name]" for robustness
//   - Alternative: IGameplayDebugger::Get().ToggleCategory() (requires module)
//   - Works with PIE (Play In Editor) for runtime debugging
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

bool UMcpAutomationBridgeSubsystem::HandleDebugAction(
    const FString& RequestId, 
    const FString& Action, 
    const TSharedPtr<FJsonObject>& Payload, 
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_debug"))
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
    // spawn_category: Toggle gameplay debugger category
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("spawn_category"))
    {
        FString CategoryName;
        if (!Payload->TryGetStringField(TEXT("categoryName"), CategoryName) ||
            CategoryName.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing or empty categoryName."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Sanitize categoryName to prevent command injection
        // Only allow alphanumeric characters and underscores
        CategoryName = CategoryName.TrimStartAndEnd();
        bool bHasInvalidChars = false;
        for (int32 i = 0; i < CategoryName.Len(); i++)
        {
            TCHAR Ch = CategoryName[i];
            if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-'))
            {
                bHasInvalidChars = true;
                break;
            }
        }
        
        if (bHasInvalidChars)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid categoryName. Only alphanumeric characters, underscores, and hyphens are allowed."),
                TEXT("INVALID_CATEGORY_NAME"));
            return true;
        }

        // Guard GEngine before Exec call
        if (!GEngine)
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Engine is not available."), TEXT("ENGINE_UNAVAILABLE"));
            return true;
        }

        // Execute via console command for robustness
        // Alternative: IGameplayDebugger::Get().ToggleCategory(CategoryName)
        // requires "GameplayDebugger" module dependency
        const FString Cmd = FString::Printf(TEXT("GameplayDebuggerCategory %s"), *CategoryName);
        const bool bCommandExecuted = GEngine->Exec(nullptr, *Cmd);

        // Check if command was executed successfully
        if (!bCommandExecuted)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to toggle gameplay debugger category: %s"), *CategoryName),
                TEXT("COMMAND_EXECUTION_FAILED"));
            return true;
        }

        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("categoryName"), CategoryName);
        Result->SetStringField(TEXT("consoleCommand"), Cmd);
        Result->SetBoolField(TEXT("commandExecuted"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Toggled gameplay debugger category: %s"), *CategoryName), 
            Result);
        return true;
    }

    // Unknown subaction
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
