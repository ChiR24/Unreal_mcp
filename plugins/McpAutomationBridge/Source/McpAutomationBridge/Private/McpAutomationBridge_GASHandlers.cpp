#include "McpAutomationBridge_GASActionRouting.h"
#include "McpAutomationBridge_GASPayloadFields.h"
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

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("GAS handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#elif !MCP_HAS_GAS
    SendAutomationError(RequestingSocket, RequestId, TEXT("GameplayAbilities plugin not enabled."), TEXT("GAS_NOT_AVAILABLE"));
    return true;
#else
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("GameplayAbilities")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("GameplayAbilities")) ||
            !FModuleManager::Get().LoadModule(TEXT("GameplayAbilities")))
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("GameplayAbilities plugin is not enabled in this project. Enable the GameplayAbilities plugin to use GAS features."),
                TEXT("GAS_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = McpGASHandlers::GetStringFieldGAS(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    McpGASHandlers::FGASRequestContext Context{
        this,
        RequestId,
        RequestingSocket,
        Payload,
        McpGASHandlers::GetStringFieldGAS(Payload, TEXT("name")),
        McpGASHandlers::GetStringFieldGAS(Payload, TEXT("path"), TEXT("/Game")),
        McpGASHandlers::GetStringFieldGAS(Payload, TEXT("blueprintPath")),
        McpGASHandlers::GetStringFieldGAS(Payload, TEXT("assetPath"))
    };

    if (McpGASHandlers::HandleGASComponents(Context, SubAction) ||
        McpGASHandlers::HandleGASAttributes(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityBasics(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityTargeting(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityTasks(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityPolicies(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsMagnitude(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsExecutionCues(Context, SubAction) ||
        McpGASHandlers::HandleGASEffectsStackingTags(Context, SubAction) ||
        McpGASHandlers::HandleGASCueNotify(Context, SubAction) ||
        McpGASHandlers::HandleGASCueEffects(Context, SubAction) ||
        McpGASHandlers::HandleGASTagAssets(Context, SubAction) ||
        McpGASHandlers::HandleGASInfo(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilitySets(Context, SubAction) ||
        McpGASHandlers::HandleGASAbilityGrantAndExecution(Context, SubAction))
    {
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown GAS subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}
