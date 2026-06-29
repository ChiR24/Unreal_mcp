#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
// FClassProperty / SetObjectPropertyValue_InContainer for the reflection-based
// CreateWidget WidgetType assignment below (no UMGEditor header required).
#include "UObject/UnrealType.h"
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

  // CreateWidget nodes carry the chosen widget class on the node's WidgetType
  // UClass property. Without it the node falls through to the generic
  // instantiation path below and spawns with a generic UUserWidget Class pin
  // and an untyped Return Value, so callers can't wire it to anything specific.
  // Resolve the requested class and assign WidgetType via reflection (no
  // UMGEditor header needed) before HandleBlueprintAddNode allocates the
  // default pins, so the typed Return Value is built. Mirrors the create_node
  // CreateWidget handling.
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
    UClass *WidgetNodeClass = ResolveClassByName(NodeType);
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
    if (FProperty *ClassProp =
            WidgetNode->GetClass()->FindPropertyByName(TEXT("WidgetType"))) {
      if (FClassProperty *TypedProp = CastField<FClassProperty>(ClassProp)) {
        TypedProp->SetObjectPropertyValue_InContainer(WidgetNode,
                                                      ResolvedWidget);
      }
    }
    return WidgetNode;
  }

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
