#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
// K2Node_ComponentBoundEvent is needed to wire a per-component delegate
// (e.g. NearMissZone.OnComponentBeginOverlap) to an event node. The header's
// public include path varies across UE versions / module layouts, so fall
// back across the known locations — same pattern used for K2Node_DynamicCast.
#if defined(__has_include)
#if __has_include("BlueprintGraph/K2Node_ComponentBoundEvent.h")
#include "BlueprintGraph/K2Node_ComponentBoundEvent.h"
#elif __has_include("BlueprintGraph/Classes/K2Node_ComponentBoundEvent.h")
#include "BlueprintGraph/Classes/K2Node_ComponentBoundEvent.h"
#elif __has_include("K2Node_ComponentBoundEvent.h")
#include "K2Node_ComponentBoundEvent.h"
#else
#define MCP_HAS_K2NODE_COMPONENTBOUNDEVENT 0
#endif
#else
#include "K2Node_ComponentBoundEvent.h"
#endif
#ifndef MCP_HAS_K2NODE_COMPONENTBOUNDEVENT
#define MCP_HAS_K2NODE_COMPONENTBOUNDEVENT 1
#endif
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
bool McpBlueprintAddEventComponentBound(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &ComponentName, const FString &DelegateEventName,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params) {
  UMcpAutomationBridgeSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;

  // Component-bound events fire when a component's multicast delegate (e.g.
  // OnComponentBeginOverlap on a SphereComponent) broadcasts. Previously
  // callers asking for K2Node_ComponentBoundEvent fell through to the custom
  // branch and got a generic Event_<guid> with no delegate binding, so the
  // event was effectively dead. Detect the request explicitly: any caller
  // that passes a componentName plus a delegate eventName (or explicitly
  // sets nodeType / eventType to K2Node_ComponentBoundEvent /
  // ComponentBoundEvent) goes through this dedicated branch.
#if MCP_HAS_K2NODE_COMPONENTBOUNDEVENT
  if (ComponentName.IsEmpty()) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Component-bound event requires a 'componentName' (the SCS "
             "component whose delegate fires, e.g. 'NearMissZone')."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (DelegateEventName.IsEmpty()) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Component-bound event requires an 'eventName' (the delegate "
             "name on the component, e.g. 'OnComponentBeginOverlap')."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Locate the SCS node by display name so we know which component the
  // delegate lives on, and to wire the bound event's ComponentPropertyName.
  USCS_Node *MatchedScsNode = nullptr;
  USimpleConstructionScript *SCS = BP->SimpleConstructionScript.Get();
  if (SCS) {
    for (USCS_Node *ScsNode : SCS->GetAllNodes()) {
      if (!ScsNode) {
        continue;
      }
      if (ScsNode->GetVariableName().ToString().Equals(ComponentName,
                                                       ESearchCase::IgnoreCase)) {
        MatchedScsNode = ScsNode;
        break;
      }
    }
  }
  if (!MatchedScsNode) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Component '%s' not found on Blueprint '%s' (SCS)."),
                        *ComponentName, *RegistryKey),
        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  UClass *ComponentClass = MatchedScsNode->ComponentClass;
  if (!ComponentClass) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("Component '%s' has no resolvable class; cannot bind a "
                 "delegate."),
            *ComponentName),
        TEXT("COMPONENT_CLASS_UNRESOLVED"));
    return true;
  }

  // Find the multicast delegate property on the component's class. We
  // accept the bare delegate name (OnComponentBeginOverlap) or the
  // generated property suffix (OnComponentBeginOverlap__DelegateSignature)
  // so callers don't need to know the engine's internal naming.
  FMulticastDelegateProperty *DelegateProp = nullptr;
  for (TFieldIterator<FMulticastDelegateProperty> PropIt(ComponentClass);
       PropIt; ++PropIt) {
    const FString PropName = PropIt->GetName();
    if (PropName.Equals(DelegateEventName, ESearchCase::IgnoreCase) ||
        PropName.StartsWith(DelegateEventName + TEXT("__"),
                            ESearchCase::IgnoreCase)) {
      DelegateProp = *PropIt;
      break;
    }
  }
  if (!DelegateProp) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("Delegate '%s' not found on component class '%s'. Expected a "
                 "multicast delegate property name like OnComponentBeginOverlap."),
            *DelegateEventName, *ComponentClass->GetName()),
        TEXT("DELEGATE_NOT_FOUND"));
    return true;
  }

  // Reuse an existing bound-event node for the same component + delegate
  // (idempotent: repeat calls don't pile up duplicates).
  UK2Node_ComponentBoundEvent *BoundEventNode = nullptr;
  for (UEdGraphNode *Node : EventGraph->Nodes) {
    if (UK2Node_ComponentBoundEvent *Existing =
            Cast<UK2Node_ComponentBoundEvent>(Node)) {
      if (Existing->ComponentPropertyName == MatchedScsNode->GetVariableName() &&
          Existing->DelegatePropertyName == DelegateProp->GetFName()) {
        BoundEventNode = Existing;
        break;
      }
    }
  }

  if (!BoundEventNode) {
    EventGraph->Modify();
    FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*EventGraph);
    BoundEventNode = NodeCreator.CreateNode();
    // InitializeComponentBoundEventParams expects the FObjectProperty that
    // represents the component variable on the owning Blueprint, and the
    // multicast delegate property on the component class. Find the
    // FObjectProperty for the component by name on the BP's generated class.
    FObjectProperty *ComponentObjProp = nullptr;
    if (BP->GeneratedClass) {
      for (TFieldIterator<FObjectProperty> PropIt(BP->GeneratedClass);
           PropIt; ++PropIt) {
        if (PropIt->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
          ComponentObjProp = *PropIt;
          break;
        }
      }
    }
    if (ComponentObjProp) {
      BoundEventNode->InitializeComponentBoundEventParams(ComponentObjProp,
                                                          DelegateProp);
    }
    BoundEventNode->ComponentPropertyName = MatchedScsNode->GetVariableName();
    BoundEventNode->DelegatePropertyName = DelegateProp->GetFName();
    BoundEventNode->DelegateOwnerClass = ComponentClass;
    BoundEventNode->NodePosX = EventPosX;
    BoundEventNode->NodePosY = EventPosY;
    NodeCreator.Finalize();
  } else {
    BoundEventNode->NodePosX = EventPosX;
    BoundEventNode->NodePosY = EventPosY;
  }

  FName EventName = BoundEventNode->CustomFunctionName;

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  McpSafeCompileBlueprint(BP);
  const bool bSaved = SaveLoadedAssetThrottled(BP);

  SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                              RegistryKey, EventName, FinalType, Params, bSaved);
  return true;
#else
  // Editor build, but K2Node_ComponentBoundEvent's header was not reachable on
  // this engine layout (MCP_HAS_K2NODE_COMPONENTBOUNDEVENT == 0). Don't let a
  // component-bound request silently fall through to the custom-event branch —
  // tell the caller the feature is not compiled in.
  Bridge.SendAutomationError(
      RequestingSocket, RequestId,
      TEXT("Component-bound events are not available in this build "
           "(K2Node_ComponentBoundEvent header was not found at compile time)."),
      TEXT("NOT_AVAILABLE"));
  return true;
#endif // MCP_HAS_K2NODE_COMPONENTBOUNDEVENT
}
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
} // namespace McpBlueprintHandlers
