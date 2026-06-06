#include "McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

bool UMcpAutomationBridgeSubsystem::HandleManageNiagaraAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_niagara_authoring"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("NiagaraEditor")) ||
            !FModuleManager::Get().LoadModule(TEXT("NiagaraEditor")))
        {
            SendAutomationError(
                RequestingSocket,
                RequestId,
                TEXT("NiagaraEditor plugin is not enabled in this project. Enable the Niagara plugin to use Niagara VFX features."),
                TEXT("NIAGARAEDITOR_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    McpNiagaraAuthoringHandlers::FActionContext Context =
        McpNiagaraAuthoringHandlers::MakeActionContext(this, RequestId, Payload, RequestingSocket);
    if (!McpNiagaraAuthoringHandlers::ValidateCommonFields(Context))
    {
        return true;
    }

    if (McpNiagaraAuthoringHandlers::HandleSystemEmitterAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleSpawnModuleAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleDynamicsModuleAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleRendererAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleParameterAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleDataInterfaceAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleEventAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleSimulationAction(Context, SubAction)) return true;
    if (McpNiagaraAuthoringHandlers::HandleInfoValidationAction(Context, SubAction)) return true;

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
