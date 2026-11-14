#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

DEFINE_LOG_CATEGORY(LogMcpLevelStructureHandlers);

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"

bool UEngineWireSubsystem::HandleManageLevelStructureAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);

    UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleManageLevelStructureAction: SubAction=%s"), *SubAction);

    // The route table hands every volume action to HandleManageVolumesAction before this runs.
    using namespace McpLevelStructure;
    using FHandler = bool (*)(UEngineWireSubsystem*, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
    static const TMap<FString, FHandler> Handlers = {
        {TEXT("create_level"), &HandleCreateLevel},
        {TEXT("create_sublevel"), &HandleCreateSublevel},
        {TEXT("configure_level_streaming"), &HandleConfigureLevelStreaming},
        {TEXT("set_streaming_distance"), &HandleSetStreamingDistance},
        {TEXT("enable_world_partition"), &HandleEnableWorldPartition},
        {TEXT("configure_grid_size"), &HandleConfigureGridSize},
        {TEXT("create_data_layer"), &HandleCreateDataLayer},
        {TEXT("assign_actor_to_data_layer"), &HandleAssignActorToDataLayer},
        {TEXT("configure_hlod_layer"), &HandleConfigureHlodLayer},
        {TEXT("create_level_instance"), &HandleCreateLevelInstance},
        {TEXT("create_packed_level_actor"), &HandleCreatePackedLevelActor},
        {TEXT("create_minimap_volume"), &HandleCreateMinimapVolume},
        {TEXT("open_level_blueprint"), &HandleOpenLevelBlueprint},
        {TEXT("add_level_blueprint_node"), &HandleAddLevelBlueprintNode},
        {TEXT("remove_level_blueprint_node"), &HandleRemoveLevelBlueprintNode},
        {TEXT("connect_level_blueprint_nodes"), &HandleConnectLevelBlueprintNodes},
        {TEXT("set_level_blueprint_pin_default"), &HandleSetLevelBlueprintPinDefault},
        {TEXT("get_level_structure_info"), &HandleGetLevelStructureInfo},
    };
    if (const FHandler* Handler = Handlers.Find(SubAction))
    {
        return (*Handler)(this, RequestId, Payload, Socket);
    }
    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown manage_level_structure action: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
}
