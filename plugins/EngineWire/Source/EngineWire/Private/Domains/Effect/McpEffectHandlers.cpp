#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Domains/Effect/McpEffectPrivate.h"

bool UEngineWireSubsystem::HandleEffectAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Action is the table's "manage_effect" or a name this function re-enters itself with (create_effect,
    // spawn_niagara, list_debug_shapes, clear_debug_shapes).
    const FString Lower = Action.ToLower();
    const bool bIsCreateEffect = Lower.Equals(TEXT("create_effect"));

    TSharedPtr<FJsonObject> LocalPayload =
        Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
    const FString NativeSubAction =
        McpEffectHandlers::NormalizeNativeSubAction(Lower, LocalPayload);

    // set_niagara_parameter on a system ASSET (assetPath, no actor) sets the user parameter's
    // default, which is set_parameter_value's job; it used to fall to the actor form and
    // answer "Actor '' not found".
    if (NativeSubAction == TEXT("set_niagara_parameter") &&
        GetJsonStringField(LocalPayload, TEXT("actorName")).IsEmpty() &&
        !McpGetFirstStringField(LocalPayload, {TEXT("assetPath"), TEXT("systemPath")}).IsEmpty())
    {
        LocalPayload->SetStringField(TEXT("subAction"), TEXT("set_parameter_value"));
        if (const TSharedPtr<FJsonValue> Value = LocalPayload->TryGetField(TEXT("value")))
        {
            LocalPayload->SetField(TEXT("parameterValue"), Value);
        }
        return HandleManageNiagaraAuthoringAction(
            RequestId, TEXT("manage_niagara_authoring"), LocalPayload, RequestingSocket);
    }

    if (McpEffectHandlers::IsNiagaraAuthoringSubAction(NativeSubAction))
    {
        return HandleManageNiagaraAuthoringAction(
            RequestId, TEXT("manage_niagara_authoring"), LocalPayload, RequestingSocket);
    }

    if (McpEffectHandlers::IsNiagaraGraphSubAction(NativeSubAction))
    {
        if (NativeSubAction == TEXT("add_niagara_module"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("add_module"));
        }
        else if (NativeSubAction == TEXT("connect_niagara_pins"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("connect_pins"));
        }
        else if (NativeSubAction == TEXT("remove_niagara_node"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("remove_node"));
        }
        return HandleNiagaraGraphAction(
            RequestId, TEXT("manage_niagara_graph"), LocalPayload, RequestingSocket);
    }

    if (Lower.Equals(TEXT("manage_effect")) && !NativeSubAction.IsEmpty())
    {
        // Catalogued lifecycle aliases: the Niagara lifecycle handler knows
        // activate_niagara / deactivate_niagara, so map the public names onto
        // them instead of falling through to "Unhandled manage_effect action".
        if (NativeSubAction == TEXT("activate") || NativeSubAction == TEXT("activate_effect"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("activate_niagara"));
            return HandleEffectAction(RequestId, TEXT("create_effect"), LocalPayload, RequestingSocket);
        }
        if (NativeSubAction == TEXT("deactivate") || NativeSubAction == TEXT("deactivate_effect"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("deactivate_niagara"));
            return HandleEffectAction(RequestId, TEXT("create_effect"), LocalPayload, RequestingSocket);
        }
        if (NativeSubAction == TEXT("reset") || NativeSubAction == TEXT("reset_effect"))
        {
            LocalPayload->SetStringField(TEXT("subAction"), TEXT("activate_niagara"));
            LocalPayload->SetBoolField(TEXT("reset"), true);
            return HandleEffectAction(RequestId, TEXT("create_effect"), LocalPayload, RequestingSocket);
        }
        const FString RoutedAction =
            (NativeSubAction == TEXT("list_debug_shapes") ||
             NativeSubAction == TEXT("clear_debug_shapes") ||
             NativeSubAction == TEXT("spawn_niagara"))
                ? NativeSubAction
                : TEXT("create_effect");
        return HandleEffectAction(RequestId, RoutedAction, LocalPayload, RequestingSocket);
    }

    McpEffectHandlers::FEffectActionContext Context{
        *this, RequestId, Action, Lower, LocalPayload, RequestingSocket};

    if (McpEffectHandlers::HandleEffectDiscoveryAction(Context))
    {
        return true;
    }

    if (bIsCreateEffect)
    {
        const FString LowerSub = McpConsolidatedActions::GetPayloadSubAction(LocalPayload);
        if (McpEffectHandlers::HandleCreateEffectSubAction(Context, LowerSub))
        {
            return true;
        }
    }

    if (McpEffectHandlers::HandleSpawnNiagara(Context, bIsCreateEffect))
    {
        return true;
    }

    if (McpEffectHandlers::HandleCleanup(Context, bIsCreateEffect))
    {
        return true;
    }

    if (McpEffectHandlers::HandleProceduralEffectAction(Context, bIsCreateEffect))
    {
        return true;
    }

    FString UnhandledAction = Action;
    LocalPayload->TryGetStringField(TEXT("action"), UnhandledAction);
    SendAutomationError(
        RequestingSocket,
        RequestId,
        FString::Printf(TEXT("Unhandled manage_effect action: %s"), *UnhandledAction),
        TEXT("UNKNOWN_ACTION"));
    return true;
}
