#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/EngineVersionComparison.h"
#include "Net/UnrealNetwork.h"
#include "UObject/NoExportTypes.h"
#include "UObject/UnrealType.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpNetworkingHandlers, Log, All);

namespace McpNetworkingHandlers
{
struct FNetworkingActionContext
{
    UMcpAutomationBridgeSubsystem& Bridge;
    const FString& RequestId;
    const TSharedPtr<FJsonObject>& Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    TSharedPtr<FJsonObject>& ResultJson;
};

UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath);
AActor* FindActorByName(UWorld* World, const FString& ActorName);
ELifetimeCondition GetReplicationCondition(const FString& ConditionStr);
ENetDormancy GetNetDormancy(const FString& DormancyStr);
ENetRole GetNetRole(const FString& RoleStr);
FString NetRoleToString(ENetRole Role);
FString NetDormancyToString(ENetDormancy Dormancy);

bool HandleSetPropertyReplicated(FNetworkingActionContext& Context);
bool HandleSetReplicationCondition(FNetworkingActionContext& Context);
bool HandleConfigureNetUpdateFrequency(FNetworkingActionContext& Context);
bool HandleConfigureNetPriority(FNetworkingActionContext& Context);
bool HandleSetNetDormancy(FNetworkingActionContext& Context);
bool HandleConfigureReplicationGraph(FNetworkingActionContext& Context);
bool HandleCreateRpcFunction(FNetworkingActionContext& Context);
bool HandleConfigureRpcValidation(FNetworkingActionContext& Context);
bool HandleSetRpcReliability(FNetworkingActionContext& Context);
bool HandleSetOwner(FNetworkingActionContext& Context);
bool HandleSetAutonomousProxy(FNetworkingActionContext& Context);
bool HandleCheckHasAuthority(FNetworkingActionContext& Context);
bool HandleCheckIsLocallyControlled(FNetworkingActionContext& Context);
bool HandleConfigureNetCullDistance(FNetworkingActionContext& Context);
bool HandleSetAlwaysRelevant(FNetworkingActionContext& Context);
bool HandleSetOnlyRelevantToOwner(FNetworkingActionContext& Context);
bool HandleConfigureNetSerialization(FNetworkingActionContext& Context);
bool HandleSetReplicatedUsing(FNetworkingActionContext& Context);
bool HandleConfigurePushModel(FNetworkingActionContext& Context);
bool HandleConfigureClientPrediction(FNetworkingActionContext& Context);
bool HandleConfigureServerCorrection(FNetworkingActionContext& Context);
bool HandleAddNetworkPredictionData(FNetworkingActionContext& Context);
bool HandleConfigureMovementPrediction(FNetworkingActionContext& Context);
bool HandleConfigureNetDriver(FNetworkingActionContext& Context);
bool HandleSetNetRole(FNetworkingActionContext& Context);
bool HandleConfigureReplicatedMovement(FNetworkingActionContext& Context);
bool HandleGetNetworkingInfo(FNetworkingActionContext& Context);
}
