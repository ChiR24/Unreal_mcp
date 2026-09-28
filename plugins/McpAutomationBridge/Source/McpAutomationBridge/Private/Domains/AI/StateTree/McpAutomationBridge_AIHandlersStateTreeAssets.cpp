#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "Domains/AI/StateTree/McpAutomationBridge_AIStateTreeFeature.h"

#include "Modules/ModuleManager.h"

namespace McpAIHandlers
{
static bool IsStateTreeModuleAvailable()
{
#if MCP_HAS_STATE_TREE
    if (FModuleManager::Get().IsModuleLoaded(TEXT("StateTreeModule")))
    {
        return true;
    }
    // Try to load it
    if (FModuleManager::Get().ModuleExists(TEXT("StateTreeModule")))
    {
        return FModuleManager::Get().LoadModule(TEXT("StateTreeModule")) != nullptr;
    }
#endif
    return false;
}

// Implements the "create_state_tree" action.
bool HandleCreateStateTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    // Runtime check: Verify StateTree module is actually loaded
    // This handles the case where headers were available at compile time
    // but the plugin is not enabled in the target project at runtime
    if (!IsStateTreeModuleAvailable())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("StateTree plugin is not enabled in this project. Enable the StateTree plugin to use State Tree features."),
            TEXT("STATETREE_PLUGIN_NOT_ENABLED"));
        return true;
    }

    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/StateTrees"));

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("State Tree name is required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    // Create the package and asset
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
        return true;
    }

    UStateTree* StateTree = NewObject<UStateTree>(Package, *Name, RF_Public | RF_Standalone);
    if (!StateTree)
    {
        Package->MarkAsGarbage();  // Prevent orphaned package leak
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create StateTree asset"), TEXT("CREATION_FAILED"));
        return true;
    }

    // Create and attach EditorData
    UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, TEXT("EditorData"), RF_Transactional);
    if (!EditorData)
    {
        StateTree->ConditionalBeginDestroy();  // Clean up StateTree before marking package as garbage
        Package->MarkAsGarbage();  // Prevent orphaned package leak
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create StateTree EditorData"), TEXT("CREATION_FAILED"));
        return true;
    }
    StateTree->EditorData = EditorData;

    // Assign schema based on type
#if MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE
    EditorData->Schema = NewObject<UStateTreeComponentSchema>(EditorData);
#else
    // UE 5.7+ or schema not available - skip schema assignment
    // The StateTree will use a default schema or require manual configuration
#endif
    // Add a default root state
    UStateTreeState& RootState = EditorData->AddRootState();
    RootState.Name = FName(TEXT("Root"));

    // Save the asset
    McpSafeAssetSave(StateTree);

    Result->SetStringField(TEXT("stateTreePath"), FullPath);
    Result->SetStringField(TEXT("rootStateName"), TEXT("Root"));
    Result->SetStringField(TEXT("message"), TEXT("State Tree created with root state"));
    McpHandlerUtils::AddVerification(Result, StateTree);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State Tree created"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("StateTree is unavailable in this build; enable the StateTree plugin (UE 5.3+)"),
        TEXT("STATE_TREE_NOT_AVAILABLE"));
#endif
    return true;
}

// Implements the "add_state_tree_state" action.
bool HandleAddStateTreeState(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString StateName = GetJsonStringField(Payload, TEXT("stateName"));
    FString ParentStateName = GetJsonStringField(Payload, TEXT("parentStateName"), TEXT("Root"));
    FString StateType = GetJsonStringField(Payload, TEXT("stateType"), TEXT("State"));

    if (StateTreePath.IsEmpty() || StateName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath and stateName are required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    // Load the StateTree
    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath), TEXT("NOT_FOUND"));
        return true;
    }

    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (!EditorData)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree has no EditorData"), TEXT("INVALID_STATE"));
        return true;
    }

    UStateTreeState* ParentState = McpFindStateTreeState(EditorData, ParentStateName);

    if (!ParentState)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Parent state '%s' not found"), *ParentStateName), TEXT("NOT_FOUND"));
        return true;
    }

    // Any EStateTreeStateType this engine has (LinkedAsset is 5.4+), case ignored; State otherwise.
    const int64 TypeValue = StaticEnum<EStateTreeStateType>()->GetValueByNameString(StateType);
    const EStateTreeStateType Type = TypeValue == INDEX_NONE ? EStateTreeStateType::State : static_cast<EStateTreeStateType>(TypeValue);

    // Add the child state
    UStateTreeState& NewState = ParentState->AddChildState(FName(*StateName), Type);

    // Save
    McpSafeAssetSave(StateTree);

    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetStringField(TEXT("parentState"), ParentStateName);
    Result->SetStringField(TEXT("stateType"), StateType);
    Result->SetStringField(TEXT("message"), TEXT("State added to StateTree"));
    McpHandlerUtils::AddVerification(Result, StateTree);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State added"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("StateTree is unavailable in this build; enable the StateTree plugin (UE 5.3+)"),
        TEXT("STATE_TREE_NOT_AVAILABLE"));
#endif
    return true;
}
}
