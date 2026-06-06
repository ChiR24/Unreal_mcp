#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeHelpersClassResolution.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
UEdGraph *FindOrCreateBlueprintNodeGraph(UBlueprint *BP,
                                         const FString &GraphName) {
  UEdGraph *TargetGraph = nullptr;
  for (UEdGraph *Graph : BP->UbergraphPages) {
    if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase)) {
      TargetGraph = Graph;
      break;
    }
  }

  if (!TargetGraph) {
    for (UEdGraph *Graph : BP->FunctionGraphs) {
      if (Graph &&
          Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase)) {
        TargetGraph = Graph;
        break;
      }
    }
  }

  if (!TargetGraph) {
    for (UEdGraph *Graph : BP->MacroGraphs) {
      if (Graph &&
          Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase)) {
        TargetGraph = Graph;
        break;
      }
    }
  }

  if (!TargetGraph &&
      GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase)) {
    TargetGraph = FBlueprintEditorUtils::CreateNewGraph(
        BP, FName(*GraphName), UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    if (TargetGraph) {
      FBlueprintEditorUtils::AddUbergraphPage(BP, TargetGraph);
    }
  }

  return TargetGraph;
}

UEdGraphNode *CreateBlueprintGraphNode(
    UEdGraph *TargetGraph, UBlueprint *BP, const FString &NodeType,
    const FString &FunctionName, const FString &VariableName,
    const FString &NodeName, FString &OutErrorMessage, FString &OutErrorCode,
    TSharedPtr<FJsonObject> &OutErrorResult) {
  const FString NodeTypeLower = NodeType.ToLower();
  if (NodeTypeLower.Contains(TEXT("callfunction")) ||
      NodeTypeLower.Contains(TEXT("function"))) {
    UK2Node_CallFunction *FuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
    if (FuncNode && !FunctionName.IsEmpty()) {
      if (UFunction *FoundFunc =
              FMcpAutomationBridge_ResolveFunction(BP, FunctionName)) {
        FuncNode->SetFromFunction(FoundFunc);
      }
    }
    return FuncNode;
  }

  if (NodeTypeLower.Contains(TEXT("variableget")) ||
      NodeTypeLower.Contains(TEXT("getvar"))) {
    UK2Node_VariableGet *VarGet = NewObject<UK2Node_VariableGet>(TargetGraph);
    if (VarGet && !VariableName.IsEmpty()) {
      VarGet->VariableReference.SetSelfMember(FName(*VariableName));
    }
    return VarGet;
  }

  if (NodeTypeLower.Contains(TEXT("variableset")) ||
      NodeTypeLower.Contains(TEXT("setvar"))) {
    UK2Node_VariableSet *VarSet = NewObject<UK2Node_VariableSet>(TargetGraph);
    if (VarSet && !VariableName.IsEmpty()) {
      VarSet->VariableReference.SetSelfMember(FName(*VariableName));
    }
    return VarSet;
  }

  if (NodeTypeLower.Contains(TEXT("customevent"))) {
    UK2Node_CustomEvent *CustomEvent = NewObject<UK2Node_CustomEvent>(TargetGraph);
    if (CustomEvent && !NodeName.IsEmpty()) {
      CustomEvent->CustomFunctionName = FName(*NodeName);
    }
    return CustomEvent;
  }

  if (NodeTypeLower.Contains(TEXT("literal"))) {
    return NewObject<UK2Node_Literal>(TargetGraph);
  }

  UClass *NodeClass = ResolveClassByName(NodeType);
  if (NodeClass && NodeClass->IsChildOf(UEdGraphNode::StaticClass())) {
    return NewObject<UEdGraphNode>(TargetGraph, NodeClass);
  }

  OutErrorResult = McpHandlerUtils::CreateResultObject();
  OutErrorResult->SetStringField(
      TEXT("error"),
      FString::Printf(TEXT("Unsupported nodeType: %s"), *NodeType));
  OutErrorMessage = TEXT("Unsupported node type (and class lookup failed)");
  OutErrorCode = TEXT("UNSUPPORTED_NODE");
  return nullptr;
}
#endif
}
