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
// blueprintPath missing -> INVALID_PARAMS, not loadable -> NOT_FOUND; both replies are sent here.
UBlueprint* LoadBlueprintOrReply(FNetworkingActionContext& Context, const FString& BlueprintPath);
// Mark the edited Blueprint modified, save it, and send the verified success reply.
bool SaveBlueprintAndReply(FNetworkingActionContext& Context, UBlueprint* Blueprint, const FString& Detail, const TCHAR* Message);
// Parses Name as a value of TEnum ("COND_OwnerOnly", "DORM_Awake", "ROLE_Authority", "Exponential"; case
// ignored). An unknown name used to fall back to a default (COND_None, DORM_Never, ROLE_None) while the reply
// echoed the caller's string, so it now fails and OutValidNames lists the accepted spellings.
template <typename TEnum>
bool TryParseNetEnum(const FString& Name, TEnum& OutValue, FString& OutValidNames)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    const int64 Value = Name.IsEmpty() ? INDEX_NONE : Enum->GetValueByNameString(Name);
    const FString Resolved = Value == INDEX_NONE ? FString() : Enum->GetNameStringByValue(Value);
    if (Value != INDEX_NONE && !Resolved.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase))
    {
        OutValue = static_cast<TEnum>(Value);
        return true;
    }
    TArray<FString> Names;
    for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
    {
        const FString EntryName = Enum->GetNameStringByIndex(Index);
        if (!EntryName.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase))
        {
            Names.Add(EntryName);
        }
    }
    OutValidNames = FString::Join(Names, TEXT(", "));
    return false;
}
// Sends INVALID_ARGUMENT naming the field, the rejected value and the accepted ones.
void ReplyInvalidEnum(FNetworkingActionContext& Context, const TCHAR* Field, const FString& Value, const FString& ValidNames);
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
bool HandleSetReplicatedUsing(FNetworkingActionContext& Context);
bool HandleConfigurePushModel(FNetworkingActionContext& Context);
bool HandleConfigureClientPrediction(FNetworkingActionContext& Context);
bool HandleConfigureServerCorrection(FNetworkingActionContext& Context);
bool HandleConfigureMovementPrediction(FNetworkingActionContext& Context);
bool HandleConfigureNetDriver(FNetworkingActionContext& Context);
bool HandleSetNetRole(FNetworkingActionContext& Context);
bool HandleConfigureReplicatedMovement(FNetworkingActionContext& Context);
bool HandleAddNetworkPredictionData(FNetworkingActionContext& Context);
bool HandleGetNetworkingInfo(FNetworkingActionContext& Context);
}
