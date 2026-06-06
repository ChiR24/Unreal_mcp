#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
namespace {
UEdGraphPin *FindCustomOrPreferredEventOutput(UEdGraph *TargetGraph) {
  const FName OnCustomName(TEXT("OnCustom"));
  for (UEdGraphNode *Node : TargetGraph->Nodes) {
    if (UK2Node_CustomEvent *Custom = Cast<UK2Node_CustomEvent>(Node)) {
      if (Custom->CustomFunctionName == OnCustomName) {
        if (UEdGraphPin *EventOutput =
                FMcpAutomationBridge_FindExecPin(Custom, EGPD_Output)) {
          return EventOutput;
        }
      }
    }
  }
  return FMcpAutomationBridge_FindPreferredEventExec(TargetGraph);
}

bool LinkVariableSetExecPin(UEdGraph *TargetGraph,
                            const UEdGraphSchema_K2 *Schema,
                            UK2Node_VariableSet *VarSet) {
  UEdGraphPin *ExecInput = FMcpAutomationBridge_FindExecPin(VarSet, EGPD_Input);
  if (!ExecInput || ExecInput->LinkedTo.Num() > 0) {
    return false;
  }

  UEdGraphPin *EventOutput = FindCustomOrPreferredEventOutput(TargetGraph);
  if (!EventOutput) {
    return false;
  }

  if (UEdGraphNode *EventNode = EventOutput->GetOwningNode()) {
    if (!EventNode->HasAnyFlags(RF_Transactional)) {
      EventNode->SetFlags(RF_Transactional);
    }
    EventNode->Modify();
  }

  if (!VarSet->HasAnyFlags(RF_Transactional)) {
    VarSet->SetFlags(RF_Transactional);
  }
  VarSet->Modify();
  const FPinConnectionResponse ExecLink =
      Schema->CanCreateConnection(EventOutput, ExecInput);
  if (ExecLink.Response == CONNECT_RESPONSE_MAKE) {
    return Schema->TryCreateConnection(EventOutput, ExecInput);
  }

  FMcpAutomationBridge_LogConnectionFailure(TEXT("blueprint_add_node exec"),
                                            EventOutput, ExecInput, ExecLink);
  return false;
}
}

void LinkBlueprintGraphNodePins(UEdGraph *TargetGraph, UEdGraphNode *NewNode,
                                bool &bExecLinked, bool &bValueLinked) {
  const UEdGraphSchema_K2 *Schema =
      Cast<UEdGraphSchema_K2>(TargetGraph->GetSchema());
  if (Schema) {
    if (UK2Node_VariableSet *VarSet = Cast<UK2Node_VariableSet>(NewNode)) {
      if (!VarSet->HasAnyFlags(RF_Transactional)) {
        VarSet->SetFlags(RF_Transactional);
      }
      VarSet->Modify();
      FMcpAutomationBridge_AttachValuePin(VarSet, TargetGraph, Schema,
                                          bValueLinked);
      bExecLinked = LinkVariableSetExecPin(TargetGraph, Schema, VarSet);
    }

    if (!bExecLinked) {
      bExecLinked =
          FMcpAutomationBridge_EnsureExecLinked(TargetGraph) || bExecLinked;
    }
  }

  if (bExecLinked) {
    TargetGraph->Modify();
  }
}
#endif
}
