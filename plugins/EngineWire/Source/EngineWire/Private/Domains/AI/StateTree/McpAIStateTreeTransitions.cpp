#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/StateTree/McpAIStateTreeFeature.h"

namespace McpAIHandlers
{
// Implements the "add_state_tree_transition" action.
bool HandleAddStateTreeTransition(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    FString StateTreePath = GetJsonStringField(Payload, TEXT("stateTreePath"));
    FString FromState = GetJsonStringField(Payload, TEXT("fromState"));
    FString ToState = GetJsonStringField(Payload, TEXT("toState"));
    FString TriggerType = GetJsonStringField(Payload, TEXT("triggerType"), TEXT("OnStateCompleted"));

    if (StateTreePath.IsEmpty() || FromState.IsEmpty() || ToState.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("stateTreePath, fromState, and toState are required"), TEXT("INVALID_PARAMS"));
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

    UStateTreeState* SourceState = McpFindStateTreeState(EditorData, FromState);
    UStateTreeState* TargetState = McpFindStateTreeState(EditorData, ToState);

    if (!SourceState)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("NOT_FOUND"));
        return true;
    }

    if (!TargetState)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("NOT_FOUND"));
        return true;
    }

    // Any EStateTreeTransitionTrigger name, case ignored; an unknown one used to become OnStateCompleted.
    const UEnum* TriggerEnum = StaticEnum<EStateTreeTransitionTrigger>();
    const int64 TriggerValue = TriggerEnum->GetValueByNameString(TriggerType);
    if (TriggerValue == INDEX_NONE || TriggerValue == 0)
    {
        TArray<FString> TriggerNames;
        for (int32 Index = 0; Index < TriggerEnum->NumEnums() - 1; ++Index)
        {
            if (TriggerEnum->GetValueByIndex(Index) != 0)
            {
                TriggerNames.Add(TriggerEnum->GetNameStringByIndex(Index));
            }
        }
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("triggerType must be one of %s."),
            *FString::Join(TriggerNames, TEXT(", "))), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    SourceState->Modify();
    FStateTreeTransition& Transition = SourceState->AddTransition(
        static_cast<EStateTreeTransitionTrigger>(TriggerValue), EStateTreeTransitionType::GotoState, TargetState);
    const FString TransitionId = Transition.ID.ToString();
    McpCompileStateTreeInto(StateTree, Result);

    Result->SetStringField(TEXT("fromState"), SourceState->Name.ToString());
    Result->SetStringField(TEXT("toState"), TargetState->Name.ToString());
    Result->SetStringField(TEXT("triggerType"), TriggerEnum->GetNameStringByValue(TriggerValue));
    Result->SetStringField(TEXT("transitionId"), TransitionId);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(StateTree));
    McpHandlerUtils::AddVerification(Result, StateTree);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transition added"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("StateTree is unavailable in this build; enable the StateTree plugin (UE 5.3+)"),
        TEXT("STATE_TREE_NOT_AVAILABLE"));
#endif
    return true;
}
}
