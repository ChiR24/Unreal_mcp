#include "Domains/GAS/McpAutomationBridge_GASActionRouting.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Modules/ModuleManager.h"

bool UMcpAutomationBridgeSubsystem::HandleManageGASAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_gas"))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    for (const TCHAR* PathField : { TEXT("abilityPath"), TEXT("effectPath"),
                                    TEXT("cuePath"), TEXT("attributeSetPath") })
    {
        if (!BlueprintPath.IsEmpty())
        {
            break;
        }
        BlueprintPath = GetJsonStringField(Payload, PathField);
    }

    McpGASHandlers::FGASRequestContext Context{
        this,
        RequestId,
        RequestingSocket,
        Payload,
        GetJsonStringField(Payload, TEXT("name")),
        GetJsonStringField(Payload, TEXT("path"), TEXT("/Game")),
        BlueprintPath,
        GetJsonStringField(Payload, TEXT("assetPath"))
    };

    if (McpGASHandlers::HandleGASComponents(Context, SubAction) ||
        McpGASHandlers::HandleGASAttributes(Context, SubAction) ||
        McpGASHandlers::HandleGASAttributeValues(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityBasics(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityTags(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityPolicies(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsMagnitude(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectModifiers(Context, SubAction) ||
        McpGASHandlers::HandleGASModifierMagnitude(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsExecutionCues(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsStackingTags(Context, SubAction) ||
        McpGASHandlers::HandleGASCueNotify(Context, SubAction) ||
        McpGASHandlers::HandleGASInfo(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilitySets(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityGrantAndExecution(Context, SubAction))
    {
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown GAS subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
}
