#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
// K2Node_ComponentBoundEvent wires a per-component delegate (e.g. OnComponentBeginOverlap) to an event node.
#include "K2Node_ComponentBoundEvent.h"

namespace McpBlueprintHandlers {
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
  if (ComponentName.IsEmpty()) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Component-bound event requires a 'componentName' (the "
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

  // The component is an object property of the generated class, whether the
  // Blueprint adds it (SCS) or inherits it (a Character's CapsuleComponent).
  // Looking only at SCS nodes refused every inherited component, and a node
  // built without that property was left with no signature: no pins and no
  // function name, a dead event reported as success. A component added since
  // the last compile has no property yet, so compile once before giving up.
  // A Widget Blueprint binds a widget's event the same way (the "On Clicked
  // (PlayButton)" node is a K2Node_ComponentBoundEvent on the widget's object
  // property), so a widget variable counts as a component too; only a widget
  // flagged Is Variable has a property at all.
  auto IsBindable = [](const UClass *PropertyClass) {
    return PropertyClass && (PropertyClass->IsChildOf(UActorComponent::StaticClass()) ||
                             PropertyClass->IsChildOf(UWidget::StaticClass()));
  };
  auto FindComponentProperty = [BP, &ComponentName, &IsBindable]() -> FObjectProperty * {
    if (!BP->GeneratedClass) {
      return nullptr;
    }
    for (TFieldIterator<FObjectProperty> PropIt(BP->GeneratedClass); PropIt; ++PropIt) {
      if (PropIt->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) && IsBindable(PropIt->PropertyClass)) {
        return *PropIt;
      }
    }
    return nullptr;
  };
  FObjectProperty *ComponentProp = FindComponentProperty();
  if (!ComponentProp) {
    McpSafeCompileBlueprint(BP);
    ComponentProp = FindComponentProperty();
  }
  if (!ComponentProp) {
    TArray<FString> Known;
    if (BP->GeneratedClass) {
      for (TFieldIterator<FObjectProperty> PropIt(BP->GeneratedClass); PropIt; ++PropIt) {
        if (IsBindable(PropIt->PropertyClass)) {
          Known.Add(PropIt->GetName());
        }
      }
    }
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Component '%s' not found on Blueprint '%s'. Its components and widget variables: %s."),
                        *ComponentName, *RegistryKey,
                        Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("<none>")),
        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }
  UClass *ComponentClass = ComponentProp->PropertyClass;

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
  // Only BlueprintAssignable delegates can drive an event, as in the editor's
  // component Events list; any other would build a node that fails to compile.
  if (DelegateProp && !DelegateProp->HasAnyPropertyFlags(CPF_BlueprintAssignable)) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Delegate '%s' on '%s' is not BlueprintAssignable, so no event can be bound to it."),
                        *DelegateProp->GetName(), *ComponentClass->GetName()),
        TEXT("DELEGATE_NOT_ASSIGNABLE"));
    return true;
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
      if (Existing->ComponentPropertyName == ComponentProp->GetFName() &&
          Existing->DelegatePropertyName == DelegateProp->GetFName()) {
        BoundEventNode = Existing;
        break;
      }
    }
  }

  if (!BoundEventNode) {
    EventGraph->Modify();
    FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*EventGraph);
    BoundEventNode = NodeCreator.CreateNode(false);
    // What the editor's "+" beside a component event does
    // (FKismetEditorUtilities::CreateNewBoundEventForClass), without opening
    // the Blueprint editor to focus the new node.
    BoundEventNode->InitializeComponentBoundEventParams(ComponentProp, DelegateProp);
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
}
} // namespace McpBlueprintHandlers
