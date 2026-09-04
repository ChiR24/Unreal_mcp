#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleEnableWorldPartition(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    bool bEnable = GetJsonBoolField(Payload, TEXT("bEnableWorldPartition"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("worldPartitionEnabled"), WorldPartition != nullptr);
    ResponseJson->SetBoolField(TEXT("requested"), bEnable);

    if (bEnable && !WorldPartition)
    {
        // Best effort in-place enable (dogfood #163): create the world partition object on the
        // world settings the way a new partitioned level does. Existing actors stay non-external;
        // a full conversion still needs the WorldPartitionConvertCommandlet.
        AWorldSettings* WorldSettings = World->GetWorldSettings();
        UWorldPartition* Created = WorldSettings ? UWorldPartition::CreateOrRepairWorldPartition(WorldSettings) : nullptr;
        if (!Created)
        {
            ResponseJson->SetStringField(TEXT("note"), TEXT("World Partition must be enabled when creating the level. Convert existing level via Edit > Convert Level"));
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("World Partition could not be created on this level; use Edit > Convert Level or create a new level with World Partition enabled"), ResponseJson, TEXT("NOT_SUPPORTED"));
            return true;
        }
        WorldSettings->MarkPackageDirty();
        World->MarkPackageDirty();
        ResponseJson->SetBoolField(TEXT("worldPartitionEnabled"), true);
        ResponseJson->SetBoolField(TEXT("requiresSaveAndReload"), true);
        ResponseJson->SetStringField(TEXT("note"), TEXT("World Partition object created on the world settings; save and reload the level. Existing actors were not converted to external actors (run the WorldPartitionConvertCommandlet for a full conversion)."));
        Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("World Partition enabled on the current level (save and reload required)"), ResponseJson);
        return true;
    }

    FString Message;
    if (WorldPartition)
    {
        Message = TEXT("World Partition is enabled for this level");
    }
    else
    {
        Message = TEXT("World Partition is not enabled for this level");
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
