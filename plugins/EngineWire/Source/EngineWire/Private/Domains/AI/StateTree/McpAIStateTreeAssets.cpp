#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/StateTree/McpAIStateTreeFeature.h"

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
bool HandleCreateStateTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    FString Error;
    FString Code;
    UClass* SchemaClass = McpFindStateTreeSchemaClass(GetJsonStringField(Payload, TEXT("schema")), Error, Code);
    // CreatePackage and NewObject rebuilt an asset already at the path, or crashed on another class.
    UStateTree* StateTree = SchemaClass ? CreateAIAssetInPackage<UStateTree>(Path, Name, TEXT("State Tree"), Error, Code) : nullptr;
    if (!StateTree)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }

    UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree, TEXT("EditorData"), RF_Transactional);
    StateTree->EditorData = EditorData;
    EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass);
    UStateTreeState& RootState = EditorData->AddRootState();
    RootState.Name = FName(TEXT("Root"));
    McpCompileStateTreeInto(StateTree, Result);

    Result->SetStringField(TEXT("stateTreePath"), StateTree->GetOutermost()->GetName());
    Result->SetStringField(TEXT("rootStateName"), TEXT("Root"));
    Result->SetStringField(TEXT("schema"), SchemaClass->GetName());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(StateTree));
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
bool HandleAddStateTreeState(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
    // States are found by name everywhere here, so a second state of one name could never be reached.
    if (McpFindStateTreeState(EditorData, StateName))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("The State Tree already has a state named %s; pick another name. Nothing was added."), *StateName), TEXT("NAME_TAKEN"));
        return true;
    }

    // Any EStateTreeStateType this engine has (LinkedAsset is 5.4+), case ignored; an unknown one used to become State.
    const UEnum* TypeEnum = StaticEnum<EStateTreeStateType>();
    const int64 TypeValue = TypeEnum->GetValueByNameString(StateType);
    if (TypeValue == INDEX_NONE)
    {
        TArray<FString> TypeNames;
        for (int32 Index = 0; Index < TypeEnum->NumEnums() - 1; ++Index)
        {
            TypeNames.Add(TypeEnum->GetNameStringByIndex(Index));
        }
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("stateType must be one of %s."),
            *FString::Join(TypeNames, TEXT(", "))), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    EditorData->Modify();
    ParentState->Modify();
    ParentState->AddChildState(FName(*StateName), static_cast<EStateTreeStateType>(TypeValue));
    McpCompileStateTreeInto(StateTree, Result);

    Result->SetStringField(TEXT("stateName"), StateName);
    Result->SetStringField(TEXT("parentState"), ParentState->Name.ToString());
    Result->SetStringField(TEXT("stateType"), TypeEnum->GetNameStringByValue(TypeValue));
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(StateTree));
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
