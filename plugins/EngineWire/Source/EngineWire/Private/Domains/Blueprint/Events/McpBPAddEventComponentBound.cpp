#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"

#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// K2Node_ComponentBoundEvent wires a per-component delegate (e.g. OnComponentBeginOverlap) to an event node.
#include "K2Node_ComponentBoundEvent.h"

namespace McpBlueprintHandlers {
bool McpBlueprintAddEventComponentBound(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &ComponentName, const FString &DelegateEventName,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params) {
  UEngineWireSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;

  // McpBPAddEvent.cpp says which requests come here.
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
  FObjectProperty *ComponentProp = McpFindComponentProperty(BP->GeneratedClass, ComponentName, /*bWidgets=*/true);
  if (!ComponentProp) {
    McpSafeCompileBlueprint(BP);
    ComponentProp = McpFindComponentProperty(BP->GeneratedClass, ComponentName, /*bWidgets=*/true);
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

  // The bare delegate name (OnComponentBeginOverlap) or its generated
  // OnComponentBeginOverlap__ spelling, as the level Blueprint's actor events take it.
  TArray<FString> Assignable;
  FMulticastDelegateProperty *DelegateProp =
      McpFindMulticastDelegate(ComponentClass, DelegateEventName, Assignable);
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
            TEXT("Delegate '%s' not found on component class '%s'; its events: %s."),
            *DelegateEventName, *ComponentClass->GetName(),
            *FString::Join(Assignable, TEXT(", "))),
        TEXT("DELEGATE_NOT_FOUND"));
    return true;
  }

  // The node already bound to this component + delegate, on any page, is reused: repeat calls don't pile up
  // duplicates, and two nodes bound to one delegate fail the compile.
  const UK2Node_ComponentBoundEvent *BoundEventNode =
      FKismetEditorUtilities::FindBoundEventForComponent(BP, DelegateProp->GetFName(), ComponentProp->GetFName());

  if (!BoundEventNode) {
    EventGraph->Modify();
    FGraphNodeCreator<UK2Node_ComponentBoundEvent> NodeCreator(*EventGraph);
    UK2Node_ComponentBoundEvent *NewNode = NodeCreator.CreateNode(false);
    // What the editor's "+" beside a component event does
    // (FKismetEditorUtilities::CreateNewBoundEventForClass), without opening
    // the Blueprint editor to focus the new node.
    NewNode->InitializeComponentBoundEventParams(ComponentProp, DelegateProp);
    NewNode->NodePosX = EventPosX;
    NewNode->NodePosY = EventPosY;
    NodeCreator.Finalize();
    BoundEventNode = NewNode;
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
