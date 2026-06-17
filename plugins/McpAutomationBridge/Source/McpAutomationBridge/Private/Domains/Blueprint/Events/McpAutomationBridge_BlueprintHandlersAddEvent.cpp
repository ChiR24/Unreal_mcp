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
