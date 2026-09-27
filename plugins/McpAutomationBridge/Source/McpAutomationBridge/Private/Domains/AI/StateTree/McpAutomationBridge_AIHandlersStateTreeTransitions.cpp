#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "Domains/AI/StateTree/McpAutomationBridge_AIStateTreeFeature.h"

namespace McpAIHandlers
{
// Implements the "add_state_tree_transition" action.
bool HandleAddStateTreeTransition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    // Any EStateTreeTransitionTrigger name (case ignored); OnStateCompleted otherwise.
    const int64 TriggerValue = StaticEnum<EStateTreeTransitionTrigger>()->GetValueByNameString(TriggerType);
    const EStateTreeTransitionTrigger Trigger = TriggerValue == INDEX_NONE
        ? EStateTreeTransitionTrigger::OnStateCompleted
        : static_cast<EStateTreeTransitionTrigger>(TriggerValue);

    // Add transition
    FStateTreeTransition& Transition = SourceState->AddTransition(Trigger, EStateTreeTransitionType::GotoState, TargetState);

    // Save
    McpSafeAssetSave(StateTree);

    Result->SetStringField(TEXT("fromState"), FromState);
    Result->SetStringField(TEXT("toState"), ToState);
    Result->SetStringField(TEXT("triggerType"), TriggerType);
    Result->SetStringField(TEXT("transitionId"), Transition.ID.ToString());
    Result->SetStringField(TEXT("message"), TEXT("Transition added"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Transition added"), Result);
#else
    Self->SendAutomationError(RequestingSocket, RequestId,
        TEXT("StateTree is unavailable in this build; enable the StateTree plugin (UE 5.3+)"),
        TEXT("STATE_TREE_NOT_AVAILABLE"));
#endif
    return true;
}
}
