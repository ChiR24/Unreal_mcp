#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_InputHandlersActionRouting.h"

#include "McpAutomationBridgeGlobals.h"
#include "Modules/ModuleManager.h"

bool UMcpAutomationBridgeSubsystem::HandleInputAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_input"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("action"), SubAction))
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing 'action' field in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
        TEXT("HandleInputAction: %s"), *SubAction);

    if (!McpInputHandlers::IsLegacyInputMappingAction(SubAction) &&
        !FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
    {
        if (!FModuleManager::Get().ModuleExists(TEXT("EnhancedInput")) ||
            !FModuleManager::Get().LoadModule(TEXT("EnhancedInput")))
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("EnhancedInput plugin is not enabled in this project. Enable the Enhanced Input plugin to use Input features."),
                TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED"));
            return true;
        }
    }

    using namespace McpInputHandlers;
    if (IsLegacyInputMappingAction(SubAction))
    {
        return HandleLegacyInputMapping(*this, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_input_action"))
    {
        return HandleCreateInputAction(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_input_mapping_context"))
    {
        return HandleCreateInputMappingContext(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("add_mapping") || SubAction == TEXT("map_input_action"))
    {
        return HandleAddInputMapping(*this, RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("remove_mapping"))
    {
        return HandleRemoveInputMapping(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("set_input_trigger"))
    {
        return HandleSetInputTrigger(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("set_input_modifier"))
    {
        return HandleSetInputModifier(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("enable_input_mapping"))
    {
        return HandleEnableInputMapping(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("disable_input_action"))
    {
        return HandleDisableInputAction(*this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("get_input_info"))
    {
        return HandleGetInputInfo(*this, RequestId, Payload, RequestingSocket);
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown sub-action: %s"), *SubAction),
        TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Input management requires Editor build."), TEXT("NOT_AVAILABLE"));
    return true;
#endif
}
