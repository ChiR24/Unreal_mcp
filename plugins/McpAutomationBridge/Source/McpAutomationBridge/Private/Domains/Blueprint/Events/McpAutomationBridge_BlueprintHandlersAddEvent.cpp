#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

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
#if WITH_EDITOR
bool HandleBlueprintAddEvent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_add_event")) ||
      ActionMatchesPattern(TEXT("add_event")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddevent")) ||
      AlphaNumLower.Contains(TEXT("addevent"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_add_event handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_add_event requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString EventType;
    LocalPayload->TryGetStringField(TEXT("eventType"), EventType);
    FString CustomName;
    LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
    const TArray<TSharedPtr<FJsonValue>> *ParamsField = nullptr;
    LocalPayload->TryGetArrayField(TEXT("parameters"), ParamsField);
    TArray<TSharedPtr<FJsonValue>> Params =
        (ParamsField && ParamsField->Num() > 0)
            ? *ParamsField
            : TArray<TSharedPtr<FJsonValue>>();

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
    if (GBlueprintBusySet.Contains(Path)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint is busy"), nullptr,
                             TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(Path);
    ON_SCOPE_EXIT {
      if (GBlueprintBusySet.Contains(Path)) {
        GBlueprintBusySet.Remove(Path);
      }
    };

    FString Normalized;
    FString LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
    const FString RegistryKey = !Normalized.IsEmpty() ? Normalized : Path;
    if (!BP) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      if (!LoadErr.IsEmpty()) {
        Err->SetStringField(TEXT("error"), LoadErr);
      }
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to load blueprint"), Err,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_add_event begin Path=%s "
                "RequestId=%s"),
           *RegistryKey, *RequestId);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("blueprint_add_event macro check: MCP_HAS_K2NODE_HEADERS=%d "
                "MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
           static_cast<int32>(MCP_HAS_K2NODE_HEADERS),
           static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

    UEdGraph *EventGraph = FBlueprintEditorUtils::FindEventGraph(BP);
    if (!EventGraph) {
      EventGraph = FBlueprintEditorUtils::CreateNewGraph(
          BP, TEXT("EventGraph"), UEdGraph::StaticClass(),
          UEdGraphSchema_K2::StaticClass());
      if (EventGraph) {
        FBlueprintEditorUtils::AddUbergraphPage(BP, EventGraph);
      }
    }

    if (!EventGraph) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create event graph"), nullptr,
                             TEXT("GRAPH_UNAVAILABLE"));
      return true;
    }

    // Read node position. posX/posY are the canonical schema params; fall back
    // to location.{x,y} then top-level x/y for raw/legacy callers. The old
    // GetIntegerField path returned 0 on absent keys, so every event piled at
    // (0,0). posX/posY take precedence.
    double PX = 0.0;
    double PY = 0.0;
    bool bHasX = Payload->TryGetNumberField(TEXT("posX"), PX);
    bool bHasY = Payload->TryGetNumberField(TEXT("posY"), PY);
    if (!bHasX || !bHasY) {
      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj &&
          LocObj->IsValid()) {
        if (!bHasX) {
          bHasX = (*LocObj)->TryGetNumberField(TEXT("x"), PX);
        }
        if (!bHasY) {
          bHasY = (*LocObj)->TryGetNumberField(TEXT("y"), PY);
        }
      }
    }
    if (!bHasX) {
      Payload->TryGetNumberField(TEXT("x"), PX);
    }
    if (!bHasY) {
      Payload->TryGetNumberField(TEXT("y"), PY);
    }
    const int32 EventPosX = static_cast<int32>(PX);
    const int32 EventPosY = static_cast<int32>(PY);

    const FString FinalType = EventType.IsEmpty() ? TEXT("custom") : EventType;
    const bool bIsCustomEvent =
        FinalType.Equals(TEXT("custom"), ESearchCase::IgnoreCase);

    FName EventName;
    UK2Node_CustomEvent *CustomEventNode = nullptr;

    // Component-bound events fire when a component's multicast delegate (e.g.
    // OnComponentBeginOverlap on a SphereComponent) broadcasts. Previously
    // callers asking for K2Node_ComponentBoundEvent fell through to the custom
    // branch and got a generic Event_<guid> with no delegate binding, so the
    // event was effectively dead. Detect the request explicitly: any caller
    // that passes a componentName plus a delegate eventName (or explicitly
    // sets nodeType / eventType to K2Node_ComponentBoundEvent /
    // ComponentBoundEvent) goes through the dedicated branch below.
    FString ComponentName;
    LocalPayload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString DelegateEventName;
    LocalPayload->TryGetStringField(TEXT("eventName"), DelegateEventName);
    FString NodeTypeHint;
    LocalPayload->TryGetStringField(TEXT("nodeType"), NodeTypeHint);
    const FString NodeTypeLower = NodeTypeHint.ToLower();
    const FString EventTypeLower = FinalType.ToLower();
    // An explicit hint (nodeType / eventType naming ComponentBoundEvent) signals
    // intent on its own; otherwise infer the request from a componentName paired
    // with a delegate eventName. Detecting the explicit hint independently means a
    // caller who names ComponentBoundEvent but omits componentName/eventName is
    // still routed here and gets a clear validation error, rather than silently
    // falling through to the custom-event branch and producing a dead
    // Event_<guid>.
    const bool bExplicitComponentBoundHint =
        NodeTypeLower.Contains(TEXT("componentboundevent")) ||
        EventTypeLower.Contains(TEXT("componentboundevent"));
    const bool bIsComponentBoundRequest =
        bExplicitComponentBoundHint ||
        (!ComponentName.IsEmpty() && !DelegateEventName.IsEmpty());

#if MCP_HAS_K2NODE_COMPONENTBOUNDEVENT
    if (bIsComponentBoundRequest) {
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
      USCS_Node* MatchedScsNode = nullptr;
      USimpleConstructionScript* SCS = BP->SimpleConstructionScript.Get();
      if (SCS) {
        for (USCS_Node* ScsNode : SCS->GetAllNodes()) {
          if (!ScsNode) {
            continue;
          }
          if (ScsNode->GetVariableName().ToString().Equals(
                  ComponentName, ESearchCase::IgnoreCase)) {
            MatchedScsNode = ScsNode;
            break;
          }
        }
      }
      if (!MatchedScsNode) {
        Bridge.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(
                TEXT("Component '%s' not found on Blueprint '%s' (SCS)."),
                *ComponentName, *RegistryKey),
            TEXT("COMPONENT_NOT_FOUND"));
        return true;
      }

      UClass* ComponentClass = MatchedScsNode->ComponentClass;
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
      FMulticastDelegateProperty* DelegateProp = nullptr;
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
                TEXT("Delegate '%s' not found on component class '%s'. "
                     "Expected a multicast delegate property name like "
                     "OnComponentBeginOverlap."),
                *DelegateEventName, *ComponentClass->GetName()),
            TEXT("DELEGATE_NOT_FOUND"));
        return true;
      }

      // Reuse an existing bound-event node for the same component + delegate
      // (idempotent: repeat calls don't pile up duplicates).
      UK2Node_ComponentBoundEvent* BoundEventNode = nullptr;
      for (UEdGraphNode* Node : EventGraph->Nodes) {
        if (UK2Node_ComponentBoundEvent* Existing =
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
        FObjectProperty* ComponentObjProp = nullptr;
        if (BP->GeneratedClass) {
          for (TFieldIterator<FObjectProperty> PropIt(BP->GeneratedClass);
               PropIt; ++PropIt) {
            if (PropIt->GetName().Equals(ComponentName,
                                         ESearchCase::IgnoreCase)) {
              ComponentObjProp = *PropIt;
              break;
            }
          }
        }
        if (ComponentObjProp) {
          BoundEventNode->InitializeComponentBoundEventParams(
              ComponentObjProp, DelegateProp);
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

      EventName = BoundEventNode->CustomFunctionName;

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
      McpSafeCompileBlueprint(BP);
      const bool bSaved = SaveLoadedAssetThrottled(BP);

      SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                                  RegistryKey, EventName, FinalType, Params,
                                  bSaved);
      return true;
    }
#else
    // Editor build, but K2Node_ComponentBoundEvent's header was not reachable on
    // this engine layout (MCP_HAS_K2NODE_COMPONENTBOUNDEVENT == 0). Don't let a
    // component-bound request silently fall through to the custom-event branch —
    // tell the caller the feature is not compiled in.
    if (bIsComponentBoundRequest) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Component-bound events are not available in this build "
               "(K2Node_ComponentBoundEvent header was not found at compile "
               "time)."),
          TEXT("NOT_AVAILABLE"));
      return true;
    }
#endif // MCP_HAS_K2NODE_COMPONENTBOUNDEVENT

    // If it's a custom event, use the existing logic
    if (bIsCustomEvent) {
      EventName = CustomName.IsEmpty()
                      ? FName(*FString::Printf(TEXT("Event_%s"),
                                               *FGuid::NewGuid().ToString()))
                      : FName(*CustomName);

      for (UEdGraphNode *Node : EventGraph->Nodes) {
        if (UK2Node_CustomEvent *ExistingNode =
                Cast<UK2Node_CustomEvent>(Node)) {
          if (ExistingNode->CustomFunctionName == EventName) {
            CustomEventNode = ExistingNode;
            break;
          }
        }
      }

      if (!CustomEventNode) {
        EventGraph->Modify();
        FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
        CustomEventNode = NodeCreator.CreateNode();
        CustomEventNode->CustomFunctionName = EventName;
        CustomEventNode->NodePosX = EventPosX;
        CustomEventNode->NodePosY = EventPosY;
        // FGraphNodeCreator::Finalize() already allocates the node's default pins
        // (OutputDelegate + then). Calling AllocateDefaultPins() again duplicated
        // them — the custom event ended up with two OutputDelegate/then pins.
        // Finalize is enough.
        NodeCreator.Finalize();
      } else {
        CustomEventNode->NodePosX = EventPosX;
        CustomEventNode->NodePosY = EventPosY;
      }

      // Handle parameters for custom events
      if (CustomEventNode && Params.Num() > 0) {
        CustomEventNode->Modify();
        // Clear existing user pins first? Or append? Assuming fresh definition.
        // For custom events, we usually manage UserDefinedPins.
        // We will just add them if they don't exist, or recreation.
        // Ideally we shouldn't wipe outputs like 'Then'.
        // Implementation: AddUserDefinedPin helper

        for (const TSharedPtr<FJsonValue> &ParamVal : Params) {
          if (!ParamVal.IsValid() || ParamVal->Type != EJson::Object)
            continue;
          const TSharedPtr<FJsonObject> ParamObj = ParamVal->AsObject();
          if (!ParamObj.IsValid())
            continue;
          FString ParamName;
          ParamObj->TryGetStringField(TEXT("name"), ParamName);
          FString ParamType;
          ParamObj->TryGetStringField(TEXT("type"), ParamType);
          // Default to Output for CustomEvent parameters (they appear as output
          // pins on the node)
          FMcpAutomationBridge_AddUserDefinedPin(CustomEventNode, ParamName,
                                                 ParamType, EGPD_Output);
        }

        CustomEventNode->ReconstructNode();
      }

    } else {
      // Standard event logic
      FString TargetEventName = FinalType;
      static TMap<FString, FString> EventNameAliases = {
          {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
          {TEXT("Tick"), TEXT("ReceiveTick")},
          {TEXT("EndPlay"), TEXT("ReceiveEndPlay")},
      };

      if (const FString *Alias = EventNameAliases.Find(TargetEventName)) {
        TargetEventName = *Alias;
      }

      EventName = FName(*TargetEventName);

      UClass *TargetClass = nullptr;
      UFunction *EventFunc = nullptr;

      // Search hierarchy
      UClass *SearchClass = BP->ParentClass;
      while (SearchClass && !EventFunc) {
        EventFunc = SearchClass->FindFunctionByName(
            *TargetEventName, EIncludeSuperFlag::ExcludeSuper);
        if (EventFunc) {
          TargetClass = SearchClass;
          break;
        }
        SearchClass = SearchClass->GetSuperClass();
      }

      if (!EventFunc) {
        Bridge.SendAutomationError(
            RequestingSocket, RequestId,
            FString::Printf(TEXT("Could not find event '%s' (resolved to '%s') "
                                 "in parent class."),
                            *FinalType, *TargetEventName),
            TEXT("EVENT_NOT_FOUND"));
        return true;
      }

      // Check if node already exists
      bool bExists = false;
      for (UEdGraphNode *Node : EventGraph->Nodes) {
        if (UK2Node_Event *EventNode = Cast<UK2Node_Event>(Node)) {
          if (EventNode->EventReference.GetMemberName() ==
              EventFunc->GetFName()) {
            bExists = true;
            break;
          }
        }
      }

      if (!bExists) {
        EventGraph->Modify();
        FGraphNodeCreator<UK2Node_Event> NodeCreator(*EventGraph);
        UK2Node_Event *EventNode = NodeCreator.CreateNode();
        EventNode->EventReference.SetFromField<UFunction>(EventFunc, false);
        EventNode->bOverrideFunction = true;
        EventNode->NodePosX = EventPosX;
        EventNode->NodePosY = EventPosY;
        NodeCreator.Finalize();
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Event %s already exists, skipping creation (idempotent "
                    "success)"),
               *TargetEventName);
        bExists = true;
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    McpSafeCompileBlueprint(BP);
    const bool bSaved = SaveLoadedAssetThrottled(BP);

    SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                                RegistryKey, EventName, FinalType, Params,
                                bSaved);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_add_event requires editor build with K2 node headers"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
  }

  // Remove an event from the blueprint (registry-backed implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
