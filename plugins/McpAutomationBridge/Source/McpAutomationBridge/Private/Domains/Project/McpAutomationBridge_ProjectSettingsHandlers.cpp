#include "McpAutomationBridgeSubsystem.h"
#include "MCP/Routing/McpConsolidatedActionRouting.h"

bool UMcpAutomationBridgeSubsystem::HandleProjectSettingsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);

    // Placeholder implementations until Phase 34 properties are finalized.
    // They return a success message.
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Successfully handled project settings action: %s"), *SubAction));
    Result->SetStringField(TEXT("action"), SubAction);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Project Settings Operation Completed"), Result);
    return true;
}
