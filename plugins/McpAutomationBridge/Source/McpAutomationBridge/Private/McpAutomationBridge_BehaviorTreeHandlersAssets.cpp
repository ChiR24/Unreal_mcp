#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeHandlersPrivate.h"
#include "McpAutomationBridge_BehaviorTreeSerializers.h"

#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "EditorAssetLibrary.h"

#if MCP_HAS_BEHAVIOR_TREE_GRAPH
#include "BehaviorTreeGraph.h"
#include "EdGraphSchema_BehaviorTree.h"
#endif

namespace McpBehaviorTreeHandlers {

bool HandleCreate(UMcpAutomationBridgeSubsystem* Subsystem,
                  const FRequestContext& Context)
{
  FString Name;
  if (!Context.Payload->TryGetStringField(TEXT("name"), Name) ||
      Name.IsEmpty()) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("name required for create"),
                                   TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SavePath;
  if (!Context.Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
      SavePath.IsEmpty()) {
    SavePath = TEXT("/Game");
  }
  if (!SavePath.StartsWith(TEXT("/"))) {
    SavePath = TEXT("/Game/") + SavePath;
  }

  const FString PackagePath = SavePath / Name;
  if (!IsValidAssetPath(PackagePath)) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."),
                        *PackagePath),
        TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(PackagePath)) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Behavior Tree already exists at %s"),
                        *PackagePath),
        TEXT("ASSET_EXISTS"));
    return true;
  }

  UPackage* Package = CreatePackage(*PackagePath);
  if (!Package) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create package"),
                                   TEXT("PACKAGE_FAILED"));
    return true;
  }

  UBehaviorTree* NewBT = NewObject<UBehaviorTree>(
      Package, UBehaviorTree::StaticClass(), FName(*Name),
      RF_Public | RF_Standalone);
  if (!NewBT) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create Behavior Tree"),
                                   TEXT("CREATE_FAILED"));
    return true;
  }

#if MCP_HAS_BEHAVIOR_TREE_GRAPH
  UEdGraph* NewGraph =
      NewObject<UBehaviorTreeGraph>(NewBT, TEXT("BehaviorTree"));
  NewGraph->Schema = UEdGraphSchema_BehaviorTree::StaticClass();
  NewBT->BTGraph = NewGraph;
  NewGraph->GetSchema()->CreateDefaultNodesForGraph(*NewGraph);
#else
  NewBT->BTGraph = nullptr;
#endif

  FAssetRegistryModule::AssetCreated(NewBT);
  Package->MarkPackageDirty();
  const bool bSaved = McpSafeAssetSave(NewBT);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), NewBT->GetPathName());
  Result->SetStringField(TEXT("name"), Name);
  Result->SetBoolField(TEXT("saved"), bSaved);
  McpHandlerUtils::AddVerification(Result, NewBT);

  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Behavior Tree created."), Result);
  return true;
}

bool HandleGetTree(UMcpAutomationBridgeSubsystem* Subsystem,
                   const FRequestContext& Context)
{
  FString AssetPath;
  if (!Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    if (!Context.Payload->TryGetStringField(TEXT("behaviorTreePath"),
                                           AssetPath) ||
        AssetPath.IsEmpty()) {
      Context.Payload->TryGetStringField(TEXT("path"), AssetPath);
    }
  }
  if (AssetPath.IsEmpty()) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        TEXT("get_tree requires 'assetPath' (or 'behaviorTreePath'/'path')."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString NormalizedPath =
      McpHandlerUtils::ValidateAssetPath(AssetPath.TrimStartAndEnd());
  if (NormalizedPath.IsEmpty()) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Invalid asset path: '%s'."), *AssetPath),
        TEXT("INVALID_PATH"));
    return true;
  }

  UBehaviorTree* BehaviorTree =
      LoadObject<UBehaviorTree>(nullptr, *NormalizedPath);
  if (!BehaviorTree) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Could not load Behavior Tree at '%s'."),
                        *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TSharedRef<FJsonObject> TreeData = MakeShared<FJsonObject>();
  McpBehaviorTreeSerializers::SerializeBehaviorTree(BehaviorTree, TreeData);
  TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetObjectField(TEXT("tree"), TreeData);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Behavior tree retrieved."), Result);
  return true;
}

}
#endif
