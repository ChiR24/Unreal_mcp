#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Domains/Effect/McpEffectPrivate.h"

bool UEngineWireSubsystem::HandleEffectAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> LocalPayload =
        Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
    // The sub-action goes back into the payload normalized: the authoring and graph handlers read it there.
    FString SubAction = McpConsolidatedActions::GetPayloadSubAction(LocalPayload);
    if (!SubAction.IsEmpty())
    {
        LocalPayload->SetStringField(TEXT("subAction"), SubAction);
    }

    // set_niagara_parameter on a system ASSET (assetPath, no actor) sets the user parameter's
    // default, which is set_parameter_value's job; it used to fall to the actor form and
    // answer "Actor '' not found".
    if (SubAction == TEXT("set_niagara_parameter") &&
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

    if (McpEffectHandlers::IsNiagaraAuthoringSubAction(SubAction))
    {
        return HandleManageNiagaraAuthoringAction(
            RequestId, TEXT("manage_niagara_authoring"), LocalPayload, RequestingSocket);
    }

    if (McpEffectHandlers::IsNiagaraGraphSubAction(SubAction))
    {
        return HandleNiagaraGraphAction(
            RequestId, TEXT("manage_niagara_graph"), LocalPayload, RequestingSocket);
    }

    // Catalogued lifecycle aliases: the Niagara lifecycle handler knows
    // activate_niagara / deactivate_niagara, so map the public names onto
    // them instead of falling through to "Unhandled manage_effect action";
    // reset is an activate that restarts the system.
    if (SubAction == TEXT("activate") || SubAction == TEXT("activate_effect") || SubAction == TEXT("reset"))
    {
        LocalPayload->SetStringField(TEXT("subAction"), TEXT("activate_niagara"));
        if (SubAction == TEXT("reset"))
        {
            LocalPayload->SetBoolField(TEXT("reset"), true);
        }
        SubAction = TEXT("activate_niagara");
    }
    else if (SubAction == TEXT("deactivate"))
    {
        LocalPayload->SetStringField(TEXT("subAction"), TEXT("deactivate_niagara"));
        SubAction = TEXT("deactivate_niagara");
    }

    McpEffectHandlers::FEffectActionContext Context{*this, RequestId, SubAction, LocalPayload, RequestingSocket};
    if (McpEffectHandlers::HandleEffectDiscoveryAction(Context) ||
        McpEffectHandlers::HandleCreateEffectSubAction(Context) ||
        McpEffectHandlers::HandleSpawnNiagara(Context) ||
        McpEffectHandlers::HandleProceduralEffectAction(Context))
    {
        return true;
    }

    SendAutomationError(
        RequestingSocket,
        RequestId,
        FString::Printf(TEXT("Unhandled manage_effect action: %s"), SubAction.IsEmpty() ? *Action : *SubAction),
        TEXT("UNKNOWN_ACTION"));
    return true;
}
