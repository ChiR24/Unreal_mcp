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
    if (CustomName.IsEmpty()) {
      // The schema documents `eventName` as "Custom event name" too. Only a
      // componentName turns eventName into a delegate name (component-bound
      // branch below); otherwise it names the custom event, instead of the
      // request silently producing a generic Event_<guid>.
      FString ComponentNameProbe;
      LocalPayload->TryGetStringField(TEXT("componentName"), ComponentNameProbe);
      if (ComponentNameProbe.IsEmpty()) {
        LocalPayload->TryGetStringField(TEXT("eventName"), CustomName);
      }
    }
    const TArray<TSharedPtr<FJsonValue>> *ParamsField = nullptr;
    LocalPayload->TryGetArrayField(TEXT("parameters"), ParamsField);
    TArray<TSharedPtr<FJsonValue>> Params =
        (ParamsField && ParamsField->Num() > 0)
            ? *ParamsField
            : TArray<TSharedPtr<FJsonValue>>();

#if MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
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

    if (bIsComponentBoundRequest) {
      return McpBlueprintAddEventComponentBound(
          Context, BP, EventGraph, EventPosX, EventPosY, RegistryKey,
          ComponentName, DelegateEventName, FinalType, Params);
    }
    if (bIsCustomEvent) {
      return McpBlueprintAddEventCustom(Context, BP, EventGraph, EventPosX,
                                        EventPosY, RegistryKey, FinalType,
                                        CustomName, Params);
    }
    return McpBlueprintAddEventStandard(Context, BP, EventGraph, EventPosX,
                                        EventPosY, RegistryKey, FinalType,
                                        Params);
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_add_event requires editor build with K2 node headers"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif // MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
  }

  return false;
}
#endif
} // namespace McpBlueprintHandlers
