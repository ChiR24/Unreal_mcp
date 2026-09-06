#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
#if MCP_HAS_EDGRAPH_SCHEMA_K2

bool FMcpAutomationBridge_AttachValuePin(UK2Node_VariableSet *VarSet,
                                                UEdGraph *Graph,
                                                const UEdGraphSchema_K2 *Schema,
                                                bool &bOutLinked) {
  if (!VarSet || !Graph || !Schema) {
    return false;
  }

  const FName VarMemberName = VarSet->VariableReference.GetMemberName();
  const FName NAME_VarSetValue(TEXT("Value"));
  UEdGraphPin *ValuePin =
      McpBlueprintUtils::FindDataPin(VarSet, EGPD_Input, VarMemberName);
  if (!ValuePin) {
    ValuePin =
        McpBlueprintUtils::FindDataPin(VarSet, EGPD_Input, NAME_VarSetValue);
  }

  if (!ValuePin) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Verbose,
        TEXT("FMcpAutomationBridge_AttachValuePin: no Value pin found on %s"),
        *VarSet->GetName());
    return false;
  }

  // Remove stale links so we can deterministically reconnect
  if (ValuePin->LinkedTo.Num() > 0) {
    Schema->BreakPinLinks(*ValuePin, true);
  }

  auto TryLinkPins = [&](UEdGraphPin *SourcePin,
                         const TCHAR *ContextLabel) -> bool {
    if (!SourcePin) {
      return false;
    }
    if (!VarSet->HasAnyFlags(RF_Transactional)) {
      VarSet->SetFlags(RF_Transactional);
    }
    VarSet->Modify();
    if (UEdGraphNode *SrcNode = SourcePin->GetOwningNode()) {
      if (!SrcNode->HasAnyFlags(RF_Transactional)) {
        SrcNode->SetFlags(RF_Transactional);
      }
      SrcNode->Modify();
    }
    const FPinConnectionResponse Response =
        Schema->CanCreateConnection(SourcePin, ValuePin);
    if (Response.Response == CONNECT_RESPONSE_MAKE) {
      if (Schema->TryCreateConnection(SourcePin, ValuePin)) {
        bOutLinked = true;
        return true;
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("%s: TryCreateConnection failed for %s"), ContextLabel,
             *VarSet->GetName());
    } else {
      McpBlueprintUtils::LogConnectionFailure(ContextLabel, SourcePin,
                                                ValuePin, Response);
    }
    return false;
  };

  bool bLinkedFromExisting = false;
  for (UEdGraphNode *Node : Graph->Nodes) {
    if (Node == VarSet) {
      continue;
    }
    if (UK2Node_VariableGet *VarGet = Cast<UK2Node_VariableGet>(Node)) {
      if (VarGet->VariableReference.GetMemberName() != VarMemberName) {
        continue;
      }
      UEdGraphPin *GetValuePin =
          McpBlueprintUtils::FindDataPin(VarGet, EGPD_Output, VarMemberName);
      if (!GetValuePin) {
        const FName NAME_VarGetValue(TEXT("Value"));
        GetValuePin = McpBlueprintUtils::FindDataPin(VarGet, EGPD_Output,
                                                       NAME_VarGetValue);
      }
      if (GetValuePin) {
        bLinkedFromExisting =
            TryLinkPins(GetValuePin, TEXT("blueprint_add_node value"));
      }
      if (bOutLinked) {
        break;
      }
    }
  }

  if (!bOutLinked) {
    // Spawn a getter when none exists and link it.
    UK2Node_VariableGet *SpawnedGet = McpBlueprintUtils::CreateVariableGetter(
        Graph, VarSet->VariableReference, VarSet->NodePosX - 250.0f,
        VarSet->NodePosY);
    if (SpawnedGet) {
      UEdGraphPin *SpawnedOutput = McpBlueprintUtils::FindDataPin(
          SpawnedGet, EGPD_Output, VarMemberName);
      if (!SpawnedOutput) {
        const FName NAME_SpawnValue(TEXT("Value"));
        SpawnedOutput = McpBlueprintUtils::FindDataPin(
            SpawnedGet, EGPD_Output, NAME_SpawnValue);
      }
      if (!TryLinkPins(SpawnedOutput,
                       TEXT("blueprint_add_node value (spawned)"))) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("blueprint_add_node value: spawned getter unable to link "
                    "for %s"),
               *VarSet->GetName());
      }
    } else {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("blueprint_add_node value: failed to spawn getter for %s"),
             *VarSet->GetName());
    }
  }

  if (!bOutLinked) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("blueprint_add_node value: unable to link value pin for %s "
                "(existing=%s)"),
           *VarSet->GetName(),
           bLinkedFromExisting ? TEXT("true") : TEXT("false"));
  }

  return bOutLinked;
}

bool FMcpAutomationBridge_EnsureExecLinked(UEdGraph *Graph) {
  if (!Graph) {
    return false;
  }

  const UEdGraphSchema_K2 *Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
  if (!Schema) {
    return false;
  }

  UEdGraphPin *EventOutput = McpBlueprintUtils::FindPreferredEventExec(Graph);
  if (!EventOutput) {
    return false;
  }

  bool bChanged = false;

  for (UEdGraphNode *Node : Graph->Nodes) {
    if (!Node || Node == EventOutput->GetOwningNode()) {
      continue;
    }

    if (Node->IsA<UK2Node_VariableSet>() || Node->IsA<UK2Node_CallFunction>()) {
      if (UEdGraphPin *ExecInput =
              McpBlueprintUtils::FindExecPin(Node, EGPD_Input)) {
        if (ExecInput && ExecInput->LinkedTo.Num() == 0) {
          if (!Node->HasAnyFlags(RF_Transactional)) {
            Node->SetFlags(RF_Transactional);
          }
          Node->Modify();
          if (UEdGraphNode *EventNode = EventOutput->GetOwningNode()) {
            if (!EventNode->HasAnyFlags(RF_Transactional)) {
              EventNode->SetFlags(RF_Transactional);
            }
            EventNode->Modify();
          }
          const FPinConnectionResponse Response =
              Schema->CanCreateConnection(EventOutput, ExecInput);
          if (Response.Response == CONNECT_RESPONSE_MAKE) {
            if (Schema->TryCreateConnection(EventOutput, ExecInput)) {
              bChanged = true;
            }
          } else {
            McpBlueprintUtils::LogConnectionFailure(
                TEXT("EnsureExecLinked"), EventOutput, ExecInput, Response);
          }
        }
      }
    }
  }

  return bChanged;
}
#endif
#endif
} // namespace McpBlueprintHandlers
