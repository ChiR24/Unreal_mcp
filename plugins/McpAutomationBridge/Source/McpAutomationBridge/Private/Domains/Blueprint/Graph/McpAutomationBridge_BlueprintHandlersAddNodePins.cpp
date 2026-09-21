#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
namespace {
UEdGraphPin *FindCustomOrPreferredEventOutput(UEdGraph *TargetGraph) {
  const FName OnCustomName(TEXT("OnCustom"));
  for (UEdGraphNode *Node : TargetGraph->Nodes) {
    if (UK2Node_CustomEvent *Custom = Cast<UK2Node_CustomEvent>(Node)) {
      if (Custom->CustomFunctionName == OnCustomName) {
        if (UEdGraphPin *EventOutput =
                McpBlueprintUtils::FindExecPin(Custom, EGPD_Output)) {
          return EventOutput;
        }
      }
    }
  }
  return McpBlueprintUtils::FindPreferredEventExec(TargetGraph);
}

bool LinkVariableSetExecPin(UEdGraph *TargetGraph,
                            const UEdGraphSchema_K2 *Schema,
                            UK2Node_VariableSet *VarSet) {
  UEdGraphPin *ExecInput = McpBlueprintUtils::FindExecPin(VarSet, EGPD_Input);
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

  McpBlueprintUtils::LogConnectionFailure(TEXT("blueprint_add_node exec"),
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
      // The value pin used to be auto-wired here: the graph was searched for
      // a Get of the SAME variable and, failing that, one was spawned. Every
      // Set therefore came out as `Set X = Get X` -- a self-assignment that
      // compiles clean, looks plausible in a node dump and silently never
      // changes the variable. Four state flags in one project were built that
      // way before anyone noticed. A freshly created Set leaves its value pin
      // unlinked on its own, which is what the editor does when you drag a
      // variable in, so the caller's set_pin_default_value (or an explicit
      // connect_pins) decides the value. Same reasoning as the exec sweep
      // removed below.
      bValueLinked = false;
      bExecLinked = LinkVariableSetExecPin(TargetGraph, Schema, VarSet);
    }
    // A graph-wide FMcpAutomationBridge_EnsureExecLinked sweep used to run
    // here. It walked EVERY node in the graph and wired any VariableSet or
    // CallFunction with a free exec input to the graph's "preferred event" --
    // so adding one node silently hung unrelated nodes off Event Tick or Event
    // PreConstruct, and the receipt's `execLinked` boolean (which the gateway
    // projects away) was the only hint. In Battle Rumble's hub that put an
    // `Add to Viewport` on Tick with a null target, logging a Blueprint runtime
    // error every frame. Only the node the caller just created is wired now,
    // and only the VariableSet convenience above; everything else is the
    // caller's own connect_pins.
  }

  if (bExecLinked) {
    TargetGraph->Modify();
  }
}
#endif
}
