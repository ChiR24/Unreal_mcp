#pragma once

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpAIHandlers, Log, All);

// Global scope on purpose: an elaborated `class UAIPerceptionComponent*`
// inside namespace McpAIHandlers declares McpAIHandlers::UAIPerceptionComponent
// and shadows the engine type for every file that includes this header.
class UAIPerceptionComponent;
class UBehaviorTree;
class UBlueprint;
class UBTNode;
class UEnvQuery;
class UEnvQueryTest;

// Whether Node is the Behavior Tree node Id names: its object name, path or node title, any case
// (BehaviorTree/McpAIBehaviorTreeAssets.cpp).
bool McpBTNodeMatchesId(const UBTNode* Node, const FString& Id);

namespace McpAIHandlers
{
inline bool SanitizeAIAssetPath(const FString& InputPath, FString& OutSanitizedPath, FString& OutError)
{
    OutSanitizedPath = SanitizeProjectRelativePath(InputPath.TrimStartAndEnd());
    if (OutSanitizedPath.IsEmpty())
    {
        OutError = McpPathRefusalMessage(TEXT("asset path"), InputPath.TrimStartAndEnd());
        return false;
    }
    return true;
}

// The AIModule Behavior Tree node class a type names: /Script/AIModule.<Prefix><Type>, and with bAlsoBare
// /Script/AIModule.<Type> before it. Null unless it is a Base subclass that can be instanced: NewObject of another
// class is a fatal check, and of an abstract one (BTTask_BlackboardBase, BTService_BlackboardBase) an ensure and an
// object the editor nulls out on save.
inline UClass* FindBTNodeClass(const TCHAR* Prefix, const FString& Type, const UClass* Base, bool bAlsoBare = false)
{
    for (const FString& Name : {bAlsoBare ? Type : FString(), Prefix + Type})
    {
        UClass* Class = Name.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *(TEXT("/Script/AIModule.") + Name));
        if (Class && Class->IsChildOf(Base) && !Class->HasAnyClassFlags(CLASS_Abstract))
        {
            return Class;
        }
    }
    return nullptr;
}

/**
 * Create a /Game AI asset of AssetClass at Path/Name, or fill OutError and OutCode
 * (ASSET_ALREADY_EXISTS for anything already at the path, saved or only in memory).
 * The caller still owns whatever comes after creation (graph seeding, saving).
 */
inline UObject* CreateAIAssetOfClass(UClass* AssetClass, const FString& Path, const FString& Name, const TCHAR* TypeNoun, FString& OutError, FString& OutCode)
{
	FString SanitizedPath;
	if (!SanitizeAIAssetPath(Path, SanitizedPath, OutError))
	{
		OutCode = TEXT("INVALID_PATH");
		return nullptr;
	}

	UPackage* Package = McpCreatePackageForNewAsset(SanitizedPath / Name, OutError, OutCode);
	if (!Package)
	{
		return nullptr;
	}

	UObject* Asset = NewObject<UObject>(Package, AssetClass, FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Failed to create %s asset"), TypeNoun);
		OutCode = TEXT("CREATION_FAILED");
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}

template <typename T>
T* CreateAIAssetInPackage(const FString& Path, const FString& Name, const TCHAR* TypeNoun, FString& OutError, FString& OutCode)
{
	return Cast<T>(CreateAIAssetOfClass(T::StaticClass(), Path, Name, TypeNoun, OutError, OutCode));
}


// The EQS query at QueryPath, or null once NOT_FOUND is sent (EQS/McpAIEQSAssets.cpp).
UEnvQuery* LoadEnvQueryOrReply(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& QueryPath);

// The AIController Blueprint at the payload's controllerPath, in any form LoadBlueprintAsset takes (OutPath: its path), or
// null once the refusal is sent: INVALID_ARGUMENT with no path, NOT_FOUND, NOT_AI_CONTROLLER for any other Blueprint
// (Controllers/McpAIControllerBehaviour.cpp).
UBlueprint* LoadControllerBlueprint(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutPath);

// The Behavior Tree at Path, or null once "Behavior Tree not found: <Path>" (NOT_FOUND) is sent
// (BehaviorTree/McpAIBehaviorTreeAssets.cpp).
UBehaviorTree* LoadBehaviorTreeOrReply(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& Path);

// A Blueprint variable's default: its default text, a compile, then the class default object's value; true when the
// compiled class has the variable and took the value (Perception/McpAIPerceptionTeam.cpp).
bool SetBPVarDefaultValueAI(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue);

// The query's test at a flat index counted across its options (empty options and tests are not counted), with the option
// it sits in and its index there when asked; null past the last test (EQS/McpAIEQSContextTests.cpp).
UEnvQueryTest* FindEQSTest(UEnvQuery* Query, int32 FlatIndex, int32* OutOptionIndex = nullptr, int32* OutIndexInOption = nullptr);

// EEnvTestFilterType by value: the names configure_test_scoring reads and get_ai_info writes.
inline constexpr const TCHAR* EQSFilterTypeNames[] = {TEXT("Minimum"), TEXT("Maximum"), TEXT("Range"), TEXT("Match")};

// Shared by the six perception actions; see Perception/McpAIPerceptionSetup.cpp.
UAIPerceptionComponent* FindOrCreatePerceptionComponent(
    UEngineWireSubsystem* Self, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UBlueprint* Blueprint, bool* OutCreated);

// The Blueprint at blueprintPath with its perception component (added when missing) opened for an edit, or null once a
// refusal went out: configure_sight_config, configure_hearing_config and configure_damage_sense_config.
UAIPerceptionComponent* LoadPerceptionOrReply(
    UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint*& OutBlueprint);

// True once ALREADY_EXISTS is sent: ComponentName is taken in the Blueprint's construction script, where CreateNode
// would pick another name while the reply named the one asked for (Navigation/McpAINavUtils.cpp).
bool RefuseTakenComponentName(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const UBlueprint* Blueprint, const FString& ComponentName);

#define MCP_AI_HANDLER_DECL(Name) bool Name(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
MCP_AI_HANDLER_DECL(HandleCreateAIController);
MCP_AI_HANDLER_DECL(HandleAssignBehaviorTree);
MCP_AI_HANDLER_DECL(HandleAssignBlackboard);
MCP_AI_HANDLER_DECL(HandleCreateBlackboardAsset);
MCP_AI_HANDLER_DECL(HandleAddBlackboardKey);
MCP_AI_HANDLER_DECL(HandleSetKeyInstanceSynced);
MCP_AI_HANDLER_DECL(HandleCreateBehaviorTree);
MCP_AI_HANDLER_DECL(HandleAddCompositeNode);
MCP_AI_HANDLER_DECL(HandleAddTaskNode);
MCP_AI_HANDLER_DECL(HandleAddDecorator);
MCP_AI_HANDLER_DECL(HandleAddService);
MCP_AI_HANDLER_DECL(HandleConfigureBehaviorTreeNode);
MCP_AI_HANDLER_DECL(HandleCreateEQSQuery);
MCP_AI_HANDLER_DECL(HandleAddEQSGenerator);
MCP_AI_HANDLER_DECL(HandleAddEQSContext);
MCP_AI_HANDLER_DECL(HandleAddEQSTest);
MCP_AI_HANDLER_DECL(HandleConfigureEQSTestScoring);
MCP_AI_HANDLER_DECL(HandleAddAIPerceptionComponent);
MCP_AI_HANDLER_DECL(HandleConfigureSightConfig);
MCP_AI_HANDLER_DECL(HandleConfigureHearingConfig);
MCP_AI_HANDLER_DECL(HandleConfigureDamageSenseConfig);
MCP_AI_HANDLER_DECL(HandleSetPerceptionTeam);
MCP_AI_HANDLER_DECL(HandleCreateStateTree);
MCP_AI_HANDLER_DECL(HandleAddStateTreeState);
MCP_AI_HANDLER_DECL(HandleAddStateTreeTransition);
MCP_AI_HANDLER_DECL(HandleConfigureStateTreeTask);
MCP_AI_HANDLER_DECL(HandleCreateSmartObjectDefinition);
MCP_AI_HANDLER_DECL(HandleAddSmartObjectSlot);
MCP_AI_HANDLER_DECL(HandleConfigureSmartObjectSlotBehavior);
MCP_AI_HANDLER_DECL(HandleAddSmartObjectComponent);
MCP_AI_HANDLER_DECL(HandleCreateMassEntityConfig);
MCP_AI_HANDLER_DECL(HandleConfigureMassEntity);
MCP_AI_HANDLER_DECL(HandleAddMassSpawner);
MCP_AI_HANDLER_DECL(HandleGetAIInfo);
MCP_AI_HANDLER_DECL(HandleGetLiveAIState); // Runtime/McpAILiveState.cpp
MCP_AI_HANDLER_DECL(HandleSetAIPerception);
MCP_AI_HANDLER_DECL(HandleCreateNavModifier);
MCP_AI_HANDLER_DECL(HandleSetAIMovement);
MCP_AI_HANDLER_DECL(HandleCreateBlackboard);
MCP_AI_HANDLER_DECL(HandleSetupPerception);
MCP_AI_HANDLER_DECL(HandleSetFocus);
MCP_AI_HANDLER_DECL(HandleClearFocus);
MCP_AI_HANDLER_DECL(HandleSetBlackboardValue);
MCP_AI_HANDLER_DECL(HandleGetBlackboardValue);
MCP_AI_HANDLER_DECL(HandleRunBehaviorTree);
MCP_AI_HANDLER_DECL(HandleStopBehaviorTree);
#undef MCP_AI_HANDLER_DECL

// get_ai_info summaries (Runtime/McpAIInfoBP.cpp, Runtime/McpAIInfoAssets.cpp
// and Runtime/McpAIEnvQueryInfo.cpp).
void DescribeAIBlueprint(class UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& AIInfo, const TSharedPtr<FJsonObject>& Result);
bool DescribeAIStateTree(const FString& StateTreePath, const TSharedPtr<FJsonObject>& Result, FString& OutError);
void AddAIAssetInventory(const TSharedPtr<FJsonObject>& Result);
void DescribeAIEnvQuery(UEnvQuery* Query, const TSharedPtr<FJsonObject>& Out);

// Reflection import shared by configure_state_tree_task and configure_mass_entity
// (Runtime/McpAIReflect.cpp).
int32 ApplyAIJsonProperties(const UStruct* Type, void* Container, const TSharedPtr<FJsonObject>& Properties, TArray<FString>& OutApplied, TArray<FString>& OutFailed);
void ListAIPropertyNames(const UStruct* Type, TArray<FString>& OutNames);
}
