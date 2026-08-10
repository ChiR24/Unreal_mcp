#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleManageCombatAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_combat"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Combat handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    McpCombatHandlers::FCombatActionContext Context{
        *this,
        RequestId,
        Action,
        Payload,
        RequestingSocket,
        GetJsonStringField(Payload, TEXT("subAction")),
        GetJsonStringField(Payload, TEXT("name")),
        GetJsonStringField(Payload, TEXT("path"), TEXT("/Game")),
        GetJsonStringField(Payload, TEXT("blueprintPath"))};

    if (Context.SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (Context.HandleWeaponCore() ||
        Context.HandleWeaponStats() ||
        Context.HandleWeaponFiring() ||
        Context.HandleWeaponHandling() ||
        Context.HandleProjectileActions() ||
        Context.HandleDamageTypes() ||
        Context.HandleDamageExecution() ||
        Context.HandleWeaponAmmo() ||
        Context.HandleWeaponEquipment() ||
        Context.HandleWeaponEffects() ||
        Context.HandleWeaponShellTrails() ||
        Context.HandleMeleeCore() ||
        Context.HandleMeleeDefense() ||
        Context.HandleInfoActions() ||
        Context.HandleHealthRuntime() ||
        Context.HandleDefenseRuntime())
    {
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown combat subAction: %s"), *Context.SubAction),
                        TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}
