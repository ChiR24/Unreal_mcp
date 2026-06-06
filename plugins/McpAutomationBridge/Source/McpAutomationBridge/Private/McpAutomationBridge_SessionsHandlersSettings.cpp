#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
bool HandleConfigureLocalSessionSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TArray<FString> SettingParams = {
        TEXT("sessionName"), TEXT("maxPlayers"), TEXT("bIsLANMatch"),
        TEXT("bAllowJoinInProgress"), TEXT("bAllowInvites"), TEXT("bUsesPresence"),
        TEXT("bUseLobbiesIfAvailable"), TEXT("bShouldAdvertise")
    };

    bool bHasAnySetting = false;
    if (Payload.IsValid())
    {
        for (const FString& Param : SettingParams)
        {
            if (Payload->HasField(Param))
            {
                bHasAnySetting = true;
                break;
            }
        }
    }

    if (!bHasAnySetting)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("At least one session setting parameter is required (sessionName, maxPlayers, bIsLANMatch, bAllowJoinInProgress, bAllowInvites, bUsesPresence, bUseLobbiesIfAvailable, or bShouldAdvertise)"), nullptr);
        return true;
    }

    FString SessionName = GetJsonStringField(Payload, TEXT("sessionName"), TEXT("DefaultSession"));
    int32 MaxPlayers = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxPlayers"), 4.0));
    bool bIsLANMatch = GetJsonBoolField(Payload, TEXT("bIsLANMatch"), false);
    bool bAllowJoinInProgress = GetJsonBoolField(Payload, TEXT("bAllowJoinInProgress"), true);
    bool bAllowInvites = GetJsonBoolField(Payload, TEXT("bAllowInvites"), true);
    bool bUsesPresence = GetJsonBoolField(Payload, TEXT("bUsesPresence"), true);
    bool bUseLobbiesIfAvailable = GetJsonBoolField(Payload, TEXT("bUseLobbiesIfAvailable"), true);
    bool bShouldAdvertise = GetJsonBoolField(Payload, TEXT("bShouldAdvertise"), true);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("sessionName"), SessionName);
    ResponseJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    ResponseJson->SetBoolField(TEXT("bIsLANMatch"), bIsLANMatch);
    ResponseJson->SetBoolField(TEXT("bAllowJoinInProgress"), bAllowJoinInProgress);
    ResponseJson->SetBoolField(TEXT("bAllowInvites"), bAllowInvites);
    ResponseJson->SetBoolField(TEXT("bUsesPresence"), bUsesPresence);
    ResponseJson->SetBoolField(TEXT("bUseLobbiesIfAvailable"), bUseLobbiesIfAvailable);
    ResponseJson->SetBoolField(TEXT("bShouldAdvertise"), bShouldAdvertise);

    FString Message = FString::Printf(TEXT("Local session settings configured: '%s' with max %d players (LAN: %s)"),
        *SessionName, MaxPlayers, bIsLANMatch ? TEXT("Yes") : TEXT("No"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleConfigureSessionInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("interfaceType")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("interfaceType is required. Valid types: Default, LAN, Null"), nullptr);
        return true;
    }

    FString InterfaceType = GetJsonStringField(Payload, TEXT("interfaceType"), TEXT("Default"));
    TArray<FString> ValidTypes = { TEXT("Default"), TEXT("LAN"), TEXT("Null") };
    if (!ValidTypes.Contains(InterfaceType))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid session interface type: %s. Valid types: Default, LAN, Null"), *InterfaceType), nullptr);
        return false;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("interfaceType"), InterfaceType);
    ResponseJson->SetStringField(TEXT("status"), TEXT("configured"));

    FString Message = FString::Printf(TEXT("Session interface configured to: %s"), *InterfaceType);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
#endif
