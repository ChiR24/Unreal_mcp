#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

bool UMcpAutomationBridgeSubsystem::HandleDebugAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_debug"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    if (SubAction == TEXT("spawn_category"))
    {
        FString CategoryName;
        Payload->TryGetStringField(TEXT("categoryName"), CategoryName);
        
        // GGameplayDebugger->ToggleCategory(CategoryName);
        // We need to access the GameplayDebugger module.
        // IGameplayDebugger::Get().ToggleCategory(CategoryName);
        // This requires "GameplayDebugger" module dependency.
        
        // We can use the console command as a robust fallback/real implementation
        // "EnableGDT" or "GameplayDebuggerCategory"
        
        FString Cmd = FString::Printf(TEXT("GameplayDebuggerCategory %s"), *CategoryName);
        GEngine->Exec(nullptr, *Cmd);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Toggled gameplay debugger category: %s"), *CategoryName));
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
