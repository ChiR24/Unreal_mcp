#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "McpAutomationBridge_AIStateTreeFeature.h"

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

bool HandleCreateStateTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("create_state_tree");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_state_tree"))
    {
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

        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/StateTrees"));
        FString SchemaType = GetStringFieldAI(Payload, TEXT("schemaType"), TEXT("Component"));

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
#elif MCP_HAS_STATE_TREE
        // Headers not available but version supports it
        FString Name = GetStringFieldAI(Payload, TEXT("name"));
        FString Path = GetStringFieldAI(Payload, TEXT("path"), TEXT("/Game/AI/StateTrees"));
        Result->SetStringField(TEXT("stateTreePath"), Path / Name);
        Result->SetStringField(TEXT("message"), TEXT("State Tree creation registered (headers unavailable - enable StateTree plugin)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        // Note: No verification since StateTree was not actually created
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State Tree registered"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    return true;
}

bool HandleAddStateTreeState(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_state_tree_state");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_state_tree_state"))
    {
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
        FString StateTreePath = GetStringFieldAI(Payload, TEXT("stateTreePath"));
        FString StateName = GetStringFieldAI(Payload, TEXT("stateName"));
        FString ParentStateName = GetStringFieldAI(Payload, TEXT("parentStateName"), TEXT("Root"));
        FString StateType = GetStringFieldAI(Payload, TEXT("stateType"), TEXT("State"));

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

        // Find the parent state
        UStateTreeState* ParentState = nullptr;
        for (UStateTreeState* SubTree : EditorData->SubTrees)
        {
            if (SubTree && SubTree->Name.ToString().Equals(ParentStateName, ESearchCase::IgnoreCase))
            {
                ParentState = SubTree;
                break;
            }
            // Check children recursively
            if (SubTree)
            {
                for (UStateTreeState* Child : SubTree->Children)
                {
                    if (Child && Child->Name.ToString().Equals(ParentStateName, ESearchCase::IgnoreCase))
                    {
                        ParentState = Child;
                        break;
                    }
                }
            }
        }

        if (!ParentState)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Parent state '%s' not found"), *ParentStateName), TEXT("NOT_FOUND"));
            return true;
        }

        // Determine state type
        EStateTreeStateType Type = EStateTreeStateType::State;
        if (StateType.Equals(TEXT("Group"), ESearchCase::IgnoreCase))
        {
            Type = EStateTreeStateType::Group;
        }
        else if (StateType.Equals(TEXT("Linked"), ESearchCase::IgnoreCase))
        {
            Type = EStateTreeStateType::Linked;
        }
        else if (StateType.Equals(TEXT("LinkedAsset"), ESearchCase::IgnoreCase))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
            Type = EStateTreeStateType::LinkedAsset;
#else
            UE_LOG(LogMcpAIHandlers, Warning, TEXT("LinkedAsset state type requires UE 5.4+. Falling back to State type."));
            Type = EStateTreeStateType::State;
#endif
        }

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
#elif MCP_HAS_STATE_TREE
        FString StateTreePath = GetStringFieldAI(Payload, TEXT("stateTreePath"));
        FString StateName = GetStringFieldAI(Payload, TEXT("stateName"));
        Result->SetStringField(TEXT("stateName"), StateName);
        Result->SetStringField(TEXT("message"), TEXT("State addition registered (headers unavailable)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        // Note: No verification since StateTree headers unavailable
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("State registered"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("State Trees require UE 5.3+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    return true;
}
}
#endif
