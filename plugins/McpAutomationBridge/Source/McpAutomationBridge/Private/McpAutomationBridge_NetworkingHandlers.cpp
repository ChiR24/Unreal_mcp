#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

DEFINE_LOG_CATEGORY(LogMcpNetworkingHandlers);

bool UMcpAutomationBridgeSubsystem::HandleManageNetworkingAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    using namespace McpNetworkingHandlers;

    if (Action != TEXT("manage_networking"))
    {
        return false;
    }

    FString SubAction = GetStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = Action;
    }

    UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("HandleManageNetworkingAction: %s"), *SubAction);

    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    FNetworkingActionContext Context{*this, RequestId, Payload, RequestingSocket, ResultJson};

    struct FNetworkingRoute
    {
        const TCHAR* Name;
        bool (*Handler)(FNetworkingActionContext& Context);
    };

    static const FNetworkingRoute Routes[] = {
        {TEXT("set_property_replicated"), HandleSetPropertyReplicated},
        {TEXT("set_replication_condition"), HandleSetReplicationCondition},
        {TEXT("configure_net_update_frequency"), HandleConfigureNetUpdateFrequency},
        {TEXT("configure_net_priority"), HandleConfigureNetPriority},
        {TEXT("set_net_dormancy"), HandleSetNetDormancy},
        {TEXT("configure_replication_graph"), HandleConfigureReplicationGraph},
        {TEXT("create_rpc_function"), HandleCreateRpcFunction},
        {TEXT("configure_rpc_validation"), HandleConfigureRpcValidation},
        {TEXT("set_rpc_reliability"), HandleSetRpcReliability},
        {TEXT("set_owner"), HandleSetOwner},
        {TEXT("set_autonomous_proxy"), HandleSetAutonomousProxy},
        {TEXT("check_has_authority"), HandleCheckHasAuthority},
        {TEXT("check_is_locally_controlled"), HandleCheckIsLocallyControlled},
        {TEXT("configure_net_cull_distance"), HandleConfigureNetCullDistance},
        {TEXT("set_always_relevant"), HandleSetAlwaysRelevant},
        {TEXT("set_only_relevant_to_owner"), HandleSetOnlyRelevantToOwner},
        {TEXT("configure_net_serialization"), HandleConfigureNetSerialization},
        {TEXT("set_replicated_using"), HandleSetReplicatedUsing},
        {TEXT("configure_push_model"), HandleConfigurePushModel},
        {TEXT("configure_client_prediction"), HandleConfigureClientPrediction},
        {TEXT("configure_server_correction"), HandleConfigureServerCorrection},
        {TEXT("add_network_prediction_data"), HandleAddNetworkPredictionData},
        {TEXT("configure_movement_prediction"), HandleConfigureMovementPrediction},
        {TEXT("configure_net_driver"), HandleConfigureNetDriver},
        {TEXT("set_net_role"), HandleSetNetRole},
        {TEXT("configure_replicated_movement"), HandleConfigureReplicatedMovement},
        {TEXT("get_networking_info"), HandleGetNetworkingInfo},
    };

    for (const FNetworkingRoute& Route : Routes)
    {
        if (SubAction == Route.Name)
        {
            return Route.Handler(Context);
        }
    }

    return false;
}
