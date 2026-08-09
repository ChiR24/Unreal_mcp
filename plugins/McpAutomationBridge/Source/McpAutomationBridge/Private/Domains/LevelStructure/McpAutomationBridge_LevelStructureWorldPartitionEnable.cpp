#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/World.h"
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
        ResponseJson->SetStringField(TEXT("note"), TEXT("World Partition must be enabled when creating the level. Convert existing level via Edit > Convert Level"));
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Cannot enable World Partition programmatically. Use 'Edit > Convert Level' in editor or create a new level with World Partition enabled."), ResponseJson);
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
