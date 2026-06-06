#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleEffectAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    const bool bIsCreateEffect =
        Lower.Equals(TEXT("create_effect")) || Lower.StartsWith(TEXT("create_effect"));
    const bool bIsNiagaraModule =
        Lower.StartsWith(TEXT("add_")) ||
        Lower.StartsWith(TEXT("set_parameter")) ||
        Lower.StartsWith(TEXT("bind_parameter")) ||
        Lower.StartsWith(TEXT("enable_gpu_simulation")) ||
        Lower.StartsWith(TEXT("configure_event"));
    const bool bIsSpawnNiagara = Lower.Equals(TEXT("spawn_niagara"));

    if (!bIsCreateEffect && !bIsNiagaraModule && !bIsSpawnNiagara &&
        !Lower.Equals(TEXT("manage_effect")) &&
        !Lower.Equals(TEXT("set_niagara_parameter")) &&
        !Lower.Equals(TEXT("list_debug_shapes")) &&
        !Lower.Equals(TEXT("clear_debug_shapes")))
    {
        return false;
    }

    TSharedPtr<FJsonObject> LocalPayload =
        Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
    const FString NativeSubAction =
        McpEffectHandlers::NormalizeNativeSubAction(Lower, Action, LocalPayload);

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
        const FString RoutedAction =
            (NativeSubAction == TEXT("list_debug_shapes") ||
             NativeSubAction == TEXT("clear_debug_shapes") ||
             NativeSubAction == TEXT("spawn_niagara") ||
             NativeSubAction == TEXT("set_niagara_parameter"))
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

    if (bIsCreateEffect || Lower.Equals(TEXT("create_niagara_system")))
    {
        const FString LowerSub =
            McpEffectHandlers::ResolveCreateEffectSubAction(Action, Lower, LocalPayload);
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

    if (McpEffectHandlers::HandleNiagaraSpawnModules(Context) ||
        McpEffectHandlers::HandleNiagaraBehaviorModules(Context) ||
        McpEffectHandlers::HandleNiagaraRenderModules(Context) ||
        McpEffectHandlers::HandleNiagaraDataEventModules(Context) ||
        McpEffectHandlers::HandleNiagaraParameterModules(Context))
    {
        return true;
    }

    FString UnhandledAction = Action;
    LocalPayload->TryGetStringField(TEXT("action"), UnhandledAction);
    SendAutomationResponse(
        RequestingSocket,
        RequestId,
        false,
        FString::Printf(TEXT("Unhandled manage_effect action: %s"), *UnhandledAction),
        nullptr,
        TEXT("UNKNOWN_ACTION"));
    return true;
}
