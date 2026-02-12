#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

bool UMcpAutomationBridgeSubsystem::HandleInsightsAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_insights"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    if (SubAction == TEXT("start_session"))
    {
        // Start trace using console command which is the standard way to control trace from editor
        // "Trace.Start"
        
        FString Channels;
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        if (Payload->TryGetStringField(TEXT("channels"), Channels) && !Channels.IsEmpty())
        {
             GEngine->Exec(nullptr, *FString::Printf(TEXT("Trace.Start %s"), *Channels));
             Result->SetStringField(TEXT("channels"), Channels);
        }
        else
        {
             GEngine->Exec(nullptr, TEXT("Trace.Start"));
        }
        Result->SetStringField(TEXT("action"), TEXT("start_trace"));
        Result->SetStringField(TEXT("status"), TEXT("started"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trace session started."), Result);
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
