#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
// UEdGraphPin::DefaultObject for the CreateWidget Class pin written below
// (no UMGEditor header required).
#include "EdGraph/EdGraphPin.h"
// K2Node_DynamicCast is not pulled in by the shared graph-compatibility
// header; include it here (with the same path fallbacks) so the cast-node
// branch in CreateBlueprintGraphNode can set TargetType.
#if defined(__has_include)
#if __has_include("BlueprintGraph/K2Node_DynamicCast.h")
#include "BlueprintGraph/K2Node_DynamicCast.h"
#elif __has_include("BlueprintGraph/Classes/K2Node_DynamicCast.h")
#include "BlueprintGraph/Classes/K2Node_DynamicCast.h"
#elif __has_include("K2Node_DynamicCast.h")
#include "K2Node_DynamicCast.h"
#endif
#else
#include "K2Node_DynamicCast.h"
#endif
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
    const FString &NodeName, const FString &TargetClass,
    FString &OutErrorMessage, FString &OutErrorCode,
    TSharedPtr<FJsonObject> &OutErrorResult) {
  const FString NodeTypeLower = NodeType.ToLower();

  // Dynamic cast nodes need their TargetType set, otherwise the node is
  // created as a "Bad cast node" with only a wildcard Object pin and no typed
  // "As <Class>" output. Previously DynamicCast fell through to the generic
  // NewObject path below, which never set TargetType, so every cast created
  // over MCP was unusable. Resolve the requested class (Blueprint asset path
  // or native class name) and assign it here.
  if (NodeTypeLower.Contains(TEXT("dynamiccast")) ||
      NodeTypeLower.Contains(TEXT("castto")) ||
      (NodeTypeLower.Contains(TEXT("cast")) && !TargetClass.IsEmpty())) {
    UK2Node_DynamicCast *CastNode =
        NewObject<UK2Node_DynamicCast>(TargetGraph);
    if (!CastNode) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorMessage = TEXT("Failed to instantiate cast node");
      OutErrorCode = TEXT("NODE_CREATION_FAILED");
      return nullptr;
    }
    if (TargetClass.IsEmpty()) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          TEXT("DynamicCast node requires a 'targetClass' (Blueprint asset "
               "path like /Game/Blueprints/BP_Cole, or a native class name)."));
      OutErrorMessage = TEXT("targetClass required for cast node");
      OutErrorCode = TEXT("INVALID_ARGUMENT");
      return nullptr;
    }
    UClass *ResolvedTarget = ResolveClassByName(TargetClass);
    if (!ResolvedTarget) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"), FString::Printf(
                             TEXT("Could not resolve targetClass '%s'"),
                             *TargetClass));
      OutErrorMessage = TEXT("Unresolved cast target class");
      OutErrorCode = TEXT("CLASS_NOT_FOUND");
      return nullptr;
    }
    CastNode->TargetType = ResolvedTarget;
    return CastNode;
  }

  // CreateWidget nodes carry the chosen widget class on their "Class" input
  // PIN, not on any UPROPERTY. Without it the node spawns classless and the
  // Blueprint stops compiling ("Spawn node Create Widget must have a class
  // specified"), and the Return Value stays a bare UUserWidget that callers
  // cannot wire to anything specific. Resolve the requested class and write
  // the pin here (no UMGEditor header needed).
  if (NodeTypeLower.Contains(TEXT("createwidget"))) {
    if (TargetClass.IsEmpty()) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          TEXT("CreateWidget node requires a 'targetClass' (Widget Blueprint "
               "asset path like /Game/Widgets/WBP_HUD, or a class name)."));
      OutErrorMessage = TEXT("targetClass required for CreateWidget node");
      OutErrorCode = TEXT("INVALID_ARGUMENT");
      return nullptr;
    }
    UClass *ResolvedWidget = ResolveClassByName(TargetClass);
    if (!ResolvedWidget) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          FString::Printf(
              TEXT("Could not resolve targetClass '%s' for CreateWidget"),
              *TargetClass));
      OutErrorMessage = TEXT("Unresolved CreateWidget target class");
      OutErrorCode = TEXT("CLASS_NOT_FOUND");
      return nullptr;
    }
    // "CreateWidget" is one of the aliases this branch matches on, but it is
    // not a UClass name, so resolving the node class from it always failed and
    // the friendly alias was unusable -- only a verbatim "K2Node_CreateWidget"
    // got through. Fall back to the real node class.
    UClass *WidgetNodeClass = ResolveClassByName(NodeType);
    if (!WidgetNodeClass ||
        !WidgetNodeClass->IsChildOf(UEdGraphNode::StaticClass())) {
      WidgetNodeClass = ResolveClassByName(TEXT("K2Node_CreateWidget"));
    }
    if (!WidgetNodeClass ||
        !WidgetNodeClass->IsChildOf(UEdGraphNode::StaticClass())) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          FString::Printf(
              TEXT("Could not resolve CreateWidget node class from nodeType "
                   "'%s'"),
              *NodeType));
      OutErrorMessage = TEXT("Unresolved CreateWidget node class");
      OutErrorCode = TEXT("UNSUPPORTED_NODE");
      return nullptr;
    }
    UEdGraphNode *WidgetNode =
        NewObject<UEdGraphNode>(TargetGraph, WidgetNodeClass);
    if (!WidgetNode) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorMessage = TEXT("Failed to instantiate CreateWidget node");
      OutErrorCode = TEXT("NODE_CREATION_FAILED");
      return nullptr;
    }
    // Allocate the pins here rather than leaving it to the caller, so the
    // Class pin exists to be written; the caller only allocates when the pin
    // list is still empty. Reconstruct afterwards so the Return Value takes
    // the concrete widget type and the exposed-on-spawn pins appear.
    WidgetNode->AllocateDefaultPins();
    UEdGraphPin *ClassPin = WidgetNode->FindPin(TEXT("Class"), EGPD_Input);
    if (!ClassPin) {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          FString::Printf(
              TEXT("'%s' has no 'Class' input pin, so the widget class could "
                   "not be set and the node would not compile."),
              *WidgetNodeClass->GetName()));
      OutErrorMessage = TEXT("CreateWidget node has no Class pin");
      OutErrorCode = TEXT("UNSUPPORTED_NODE");
      return nullptr;
    }
    ClassPin->DefaultObject = ResolvedWidget;
    ClassPin->DefaultValue.Reset();
    WidgetNode->ReconstructNode();
    return WidgetNode;
  }

  if (NodeTypeLower.Contains(TEXT("callfunction")) ||
      NodeTypeLower.Contains(TEXT("function"))) {
    UK2Node_CallFunction *FuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
    if (FuncNode && !FunctionName.IsEmpty()) {
      UFunction *FoundFunc = FMcpAutomationBridge_ResolveFunction(BP, FunctionName);
      // An unresolved name used to fall through and leave a bound-to-nothing
      // node in the graph: the call answered "Node added", and the only sign
      // was a compiler error about a function named "None" further down. Refuse
      // instead, and say where the name was looked for.
      if (!FoundFunc) {
        OutErrorResult = McpHandlerUtils::CreateResultObject();
        OutErrorResult->SetStringField(
            TEXT("error"),
            FString::Printf(
                TEXT("No function named '%s' is callable from '%s'. Member "
                     "functions of another class need create_node with "
                     "memberName plus memberClass (e.g. memberName "
                     "'SetText', memberClass '/Script/UMG.TextBlock')."),
                *FunctionName, *BP->GetName()));
        OutErrorMessage = TEXT("Unresolved function name");
        OutErrorCode = TEXT("FUNCTION_NOT_FOUND");
        return nullptr;
      }
      FuncNode->SetFromFunction(FoundFunc);
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
  if (!NodeClass) {
    // Resolve the documented friendly aliases (Branch, Sequence, Delay, ...)
    // through the same catalog create_node uses, so add_node {nodeType:"Branch"}
    // no longer fails UNSUPPORTED_NODE while create_node succeeds.
    NodeClass = McpBlueprintGraphHandlers::FindNodeClassByName(NodeType);
  }
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
