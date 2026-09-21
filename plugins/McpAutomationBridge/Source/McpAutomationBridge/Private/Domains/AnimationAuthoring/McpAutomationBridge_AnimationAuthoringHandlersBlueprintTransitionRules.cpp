#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Kismet/KismetMathLibrary.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

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

template <typename TNode> TNode *AddRuleNode(UEdGraph *RuleGraph) {
  TNode *Node = NewObject<TNode>(RuleGraph);
  Node->CreateNewGuid();
  Node->PostPlacedNewNode();
  Node->AllocateDefaultPins();
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

TSharedPtr<FJsonObject> HandleBlueprintTransitionRuleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_transition_rules"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetJsonStringField(Params, TEXT("stateMachineName"), TEXT(""));
        FString FromState = GetJsonStringField(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetJsonStringField(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetJsonNumberField(Params, TEXT("crossfadeDuration"), -1.0));
        int32 PriorityOrder = static_cast<int32>(GetJsonNumberField(Params, TEXT("priorityOrder"), -1));
        bool bAutomatic = GetJsonBoolField(Params, TEXT("automaticRule"), false);
        bool bBidirectional = GetJsonBoolField(Params, TEXT("bidirectional"), false);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        FString ConditionVariable = GetJsonStringField(Params, TEXT("conditionVariable"), TEXT(""));
        FString ConditionComparison = GetJsonStringField(Params, TEXT("conditionComparison"), TEXT("greater")).ToLower();
        double ConditionValue = GetJsonNumberField(Params, TEXT("conditionValue"), 0.0);

        // Try to find in-memory version first (may have unsaved changes)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        TArray<UAnimGraphNode_StateMachine*> MatchingStateMachines = FindStateMachineNodes(AnimGraph, StateMachineName);
        if (MatchingStateMachines.Num() == 0)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }

        UAnimStateTransitionNode* TransNode = nullptr;
        for (UAnimGraphNode_StateMachine* MatchingSMNode : MatchingStateMachines)
        {
            if (!MatchingSMNode || !MatchingSMNode->EditorStateMachineGraph)
            {
                continue;
            }

            UAnimationStateMachineGraph* CandidateGraph = Cast<UAnimationStateMachineGraph>(MatchingSMNode->EditorStateMachineGraph);
            if (!CandidateGraph)
            {
                continue;
            }

            TransNode = FindTransitionNode(CandidateGraph, FromState, ToState);
            if (TransNode)
            {
                break;
            }
        }

        if (!TransNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState), TEXT("TRANSITION_NOT_FOUND"));
        }

        // Update transition properties
        if (CrossfadeDuration >= 0.0f)
        {
            TransNode->CrossfadeDuration = CrossfadeDuration;
        }
        if (PriorityOrder >= 0)
        {
            TransNode->PriorityOrder = PriorityOrder;
        }
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
        TransNode->Bidirectional = bBidirectional;

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
            FString::Printf(TEXT("Cannot update transition rules for '%s' -> '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
