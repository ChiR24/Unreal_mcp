#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Kismet/KismetMathLibrary.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

namespace {
// An Animation Blueprint edited earlier in the same session is only correct in
// memory, so prefer that over whatever is still on disk.
UAnimBlueprint *LoadAnimBlueprintForMcp(const FString &BlueprintPath) {
  if (UAnimBlueprint *InMemory = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath)) {
    return InMemory;
  }
  return Cast<UAnimBlueprint>(
      StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
}
} // namespace

#if MCP_HAS_K2NODE_HEADERS && MCP_HAS_ANIM_STATE_TRANSITION
namespace {
// A transition whose rule graph leaves bCanEnterTransition unconnected is
// permanently false, so a state machine authored over MCP could be built
// correctly and still never leave its entry state -- reported as success the
// whole way. This writes the rule the caller asked for into that graph.
UEdGraphPin *FindCanEnterPin(UEdGraph *RuleGraph) {
  for (UEdGraphNode *Node : RuleGraph->Nodes) {
    if (UAnimGraphNode_TransitionResult *Result =
            Cast<UAnimGraphNode_TransitionResult>(Node)) {
      return Result->FindPin(TEXT("bCanEnterTransition"));
    }
  }
  return nullptr;
}

// Deliberately does NOT allocate pins: a VariableGet names its output pin
// after the variable and a CallFunction builds its pins from the function, so
// both have to be pointed at their target first. Allocating here and again
// after that setup duplicates every pin.
template <typename TNode> TNode *AddRuleNode(UEdGraph *RuleGraph) {
  TNode *Node = NewObject<TNode>(RuleGraph);
  Node->CreateNewGuid();
  Node->PostPlacedNewNode();
  RuleGraph->AddNode(Node, false, false);
  return Node;
}

// The float comparisons live on UKismetMathLibrary under names that do not
// match the operator spelling, so map the words a caller would use.
FName ComparisonFunctionName(const FString &Comparison) {
  if (Comparison == TEXT("less")) { return TEXT("Less_DoubleDouble"); }
  if (Comparison == TEXT("greater_equal")) { return TEXT("GreaterEqual_DoubleDouble"); }
  if (Comparison == TEXT("less_equal")) { return TEXT("LessEqual_DoubleDouble"); }
  return TEXT("Greater_DoubleDouble");
}

bool BuildTransitionRule(UEdGraph *RuleGraph, UAnimBlueprint *AnimBP,
                         const FString &VariableName, const FString &Comparison,
                         double Value, FString &OutError) {
  UEdGraphPin *CanEnter = FindCanEnterPin(RuleGraph);
  if (CanEnter == nullptr) {
    OutError = TEXT("Transition rule graph has no result node");
    return false;
  }
  CanEnter->BreakAllPinLinks();

  UK2Node_VariableGet *Get = AddRuleNode<UK2Node_VariableGet>(RuleGraph);
  Get->VariableReference.SetSelfMember(FName(*VariableName));
  Get->AllocateDefaultPins();
  UEdGraphPin *ValuePin = Get->FindPin(FName(*VariableName));
  if (ValuePin == nullptr) {
    OutError = FString::Printf(
        TEXT("'%s' is not a variable on this Animation Blueprint"), *VariableName);
    return false;
  }

  // A bool variable drives the pin directly; anything numeric needs a compare.
  if (Comparison == TEXT("true") || Comparison == TEXT("false")) {
    if (Comparison == TEXT("false")) {
      UK2Node_CallFunction *Not = AddRuleNode<UK2Node_CallFunction>(RuleGraph);
      Not->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(
          TEXT("Not_PreBool")));
      Not->AllocateDefaultPins();
      ValuePin->MakeLinkTo(Not->FindPin(TEXT("A")));
      ValuePin = Not->GetReturnValuePin();
    }
    ValuePin->MakeLinkTo(CanEnter);
  } else {
    UK2Node_CallFunction *Compare = AddRuleNode<UK2Node_CallFunction>(RuleGraph);
    Compare->SetFromFunction(
        UKismetMathLibrary::StaticClass()->FindFunctionByName(
            ComparisonFunctionName(Comparison)));
    Compare->AllocateDefaultPins();
    UEdGraphPin *APin = Compare->FindPin(TEXT("A"));
    UEdGraphPin *BPin = Compare->FindPin(TEXT("B"));
    if (APin == nullptr || BPin == nullptr) {
      OutError = TEXT("Comparison node did not expose its operands");
      return false;
    }
    ValuePin->MakeLinkTo(APin);
    BPin->DefaultValue = FString::SanitizeFloat(Value);
    Compare->GetReturnValuePin()->MakeLinkTo(CanEnter);
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
  return true;
}
} // namespace
#endif

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
namespace {
// One name can match more than one state machine node in the AnimGraph, so the
// transition is looked for in each of them rather than only the first.
UAnimStateTransitionNode *FindTransitionInMachines(UEdGraph *AnimGraph,
                                                   const FString &StateMachineName,
                                                   const FString &FromState,
                                                   const FString &ToState) {
  for (UAnimGraphNode_StateMachine *Machine :
       FindStateMachineNodes(AnimGraph, StateMachineName)) {
    if (Machine == nullptr || Machine->EditorStateMachineGraph == nullptr) {
      continue;
    }
    UAnimationStateMachineGraph *Graph =
        Cast<UAnimationStateMachineGraph>(Machine->EditorStateMachineGraph);
    if (Graph == nullptr) {
      continue;
    }
    if (UAnimStateTransitionNode *Found = FindTransitionNode(Graph, FromState, ToState)) {
      return Found;
    }
  }
  return nullptr;
}
} // namespace
#endif

TSharedPtr<FJsonObject> HandleBlueprintTransitionRuleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const bool bDeleteTransition = SubAction == TEXT("delete_transition");
    if (SubAction != TEXT("set_transition_rules") && !bDeleteTransition)
    {
        return nullptr;
    }

    FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
    FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"), TEXT(""));
    FString FromState = GetJsonStringField(Params, TEXT("fromState"), TEXT(""));
    FString ToState = GetJsonStringField(Params, TEXT("toState"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    UAnimBlueprint* AnimBP = LoadAnimBlueprintForMcp(BlueprintPath);
    if (!AnimBP)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
    }

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
    UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
    if (!AnimGraph)
    {
        ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
    }

    if (FindStateMachineNodes(AnimGraph, StateMachineName).Num() == 0)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
    }

    UAnimStateTransitionNode* TransNode = FindTransitionInMachines(AnimGraph, StateMachineName, FromState, ToState);
    if (!TransNode)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState), TEXT("TRANSITION_NOT_FOUND"));
    }

    // Without this there was no way to take a wrong transition back out of a
    // state machine: the only escape was to leave it wired with a condition
    // that can never be true.
    if (bDeleteTransition)
    {
        TransNode->DestroyNode();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition '%s' -> '%s' deleted"), *FromState, *ToState));
        Response->SetBoolField(TEXT("deleted"), true);
        return Response;
    }

    // crossfadeDuration is the native spelling; blendTime is what the capability
    // record declares, and it used to be read by neither -- so blend timing was
    // accepted and silently dropped.
    double Crossfade = GetJsonNumberField(Params, TEXT("crossfadeDuration"), -1.0);
    if (Crossfade < 0.0)
    {
        Crossfade = GetJsonNumberField(Params, TEXT("blendTime"), -1.0);
    }
    int32 PriorityOrder = static_cast<int32>(GetJsonNumberField(Params, TEXT("priorityOrder"), -1));
    FString ConditionVariable = GetJsonStringField(Params, TEXT("conditionVariable"), TEXT(""));
    FString ConditionComparison = GetJsonStringField(Params, TEXT("conditionComparison"), TEXT("greater")).ToLower();
    double ConditionValue = GetJsonNumberField(Params, TEXT("conditionValue"), 0.0);

    if (Crossfade >= 0.0)
    {
        TransNode->CrossfadeDuration = static_cast<float>(Crossfade);
    }
    if (PriorityOrder >= 0)
    {
        TransNode->PriorityOrder = PriorityOrder;
    }
    // Assigning these unconditionally meant a later call that only changed the
    // condition silently reset whatever the caller had set them to.
    if (Params->HasField(TEXT("automaticRule")))
    {
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = GetJsonBoolField(Params, TEXT("automaticRule"), false);
    }
    if (Params->HasField(TEXT("bidirectional")))
    {
        TransNode->Bidirectional = GetJsonBoolField(Params, TEXT("bidirectional"), false);
    }

    if (!ConditionVariable.IsEmpty())
    {
#if MCP_HAS_K2NODE_HEADERS
        FString RuleError;
        if (!BuildTransitionRule(TransNode->GetBoundGraph(), AnimBP, ConditionVariable,
                                 ConditionComparison, ConditionValue, RuleError))
        {
            ANIM_ERROR_RESPONSE(RuleError, TEXT("TRANSITION_RULE_FAILED"));
        }
        Response->SetStringField(TEXT("condition"),
            FString::Printf(TEXT("%s %s %s"), *ConditionVariable, *ConditionComparison,
                            *FString::SanitizeFloat(ConditionValue)));
#else
        ANIM_ERROR_RESPONSE(TEXT("Transition conditions need the BlueprintGraph K2Node headers"), TEXT("K2NODE_UNAVAILABLE"));
#endif
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
    SaveAnimAsset(AnimBP, bSave);

    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition rules updated for '%s' -> '%s'"), *FromState, *ToState));
#else
    // AnimGraph headers not available - return error instead of fake success
    ANIM_ERROR_RESPONSE(
        FString::Printf(TEXT("Cannot update transition '%s' -> '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
        TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
    return Response;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
