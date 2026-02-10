#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/AutomationTest.h"

bool UMcpAutomationBridgeSubsystem::HandleTestAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_tests"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    if (SubAction == TEXT("run_tests"))
    {
        FString Filter;
        Payload->TryGetStringField(TEXT("filter"), Filter);
        
        // FAutomationTestFramework::Get().StartTestByName(Filter, ...);
        // We will start the test. Results are async, but we can confirm start.
        // To get results, we would need to bind to OnTestEnd, which is a global delegate.
        // For this bridge, we'll just start it.
        
        FAutomationTestFramework::Get().StartTestByName(Filter, 0);
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("action"), TEXT("run_tests"));
        Result->SetStringField(TEXT("filter"), Filter);
        Result->SetBoolField(TEXT("started"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tests started. Check logs for results."), Result);
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
