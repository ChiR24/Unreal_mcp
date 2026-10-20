#pragma once

#include "EngineWireSubsystem.h"
#include "Core/Module/EngineWireGlobals.h"
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
class UBlueprint;

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


/**
 * Create a /Game AI asset of type T at Path/Name, or fill OutError and OutCode
 * (ASSET_ALREADY_EXISTS for anything already at the path, saved or only in memory).
 * The caller still owns whatever comes after creation (graph seeding, saving).
 */
template <typename T>
T* CreateAIAssetInPackage(const FString& Path, const FString& Name, const TCHAR* TypeNoun, FString& OutError, FString& OutCode)
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

	T* Asset = NewObject<T>(Package, T::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Failed to create %s asset"), TypeNoun);
		OutCode = TEXT("CREATION_FAILED");
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}


// Shared by the five perception actions; see PerceptionSetup.cpp.
UAIPerceptionComponent* FindOrCreatePerceptionComponent(
    UEngineWireSubsystem* Self, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UBlueprint* Blueprint, bool* OutCreated);

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

// get_ai_info summaries (Runtime/McpAIInfoBP.cpp
// and Runtime/McpAIInfoAssets.cpp).
void DescribeAIBlueprint(class UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& AIInfo, const TSharedPtr<FJsonObject>& Result);
bool DescribeAIStateTree(const FString& StateTreePath, const TSharedPtr<FJsonObject>& Result, FString& OutError);
void AddAIAssetInventory(const TSharedPtr<FJsonObject>& Result);

// Reflection import shared by configure_state_tree_task and configure_mass_entity
// (Runtime/McpAIReflect.cpp).
int32 ApplyAIJsonProperties(const UStruct* Type, void* Container, const TSharedPtr<FJsonObject>& Properties, TArray<FString>& OutApplied, TArray<FString>& OutFailed);
void ListAIPropertyNames(const UStruct* Type, TArray<FString>& OutNames);
}
