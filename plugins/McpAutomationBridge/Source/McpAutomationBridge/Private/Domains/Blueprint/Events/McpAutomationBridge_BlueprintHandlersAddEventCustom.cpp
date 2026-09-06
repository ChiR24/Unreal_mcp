#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
bool McpBlueprintAddEventCustom(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &FinalType, const FString &CustomName,
    const TArray<TSharedPtr<FJsonValue>> &Params) {
  UMcpAutomationBridgeSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;

  FName EventName = CustomName.IsEmpty()
                        ? FName(*FString::Printf(TEXT("Event_%s"),
                                                 *FGuid::NewGuid().ToString()))
                        : FName(*CustomName);

  UK2Node_CustomEvent *CustomEventNode = nullptr;
  for (UEdGraphNode *Node : EventGraph->Nodes) {
    if (UK2Node_CustomEvent *ExistingNode = Cast<UK2Node_CustomEvent>(Node)) {
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

  // Handle parameters for custom events.
  if (CustomEventNode && Params.Num() > 0) {
    CustomEventNode->Modify();
    // Clear existing user pins first? Or append? Assuming fresh definition.
    // For custom events, we usually manage UserDefinedPins.
    // We will just add them if they don't exist, or recreation.
    // Ideally we shouldn't wipe outputs like 'Then'.
    // Implementation: AddUserDefinedPin helper.

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
      // pins on the node).
      FMcpAutomationBridge_AddUserDefinedPin(CustomEventNode, ParamName,
                                             ParamType, EGPD_Output);
    }

    CustomEventNode->ReconstructNode();
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  McpSafeCompileBlueprint(BP);
  const bool bSaved = SaveLoadedAssetThrottled(BP);

  SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                              RegistryKey, EventName, FinalType, Params, bSaved);
  return true;
}
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
} // namespace McpBlueprintHandlers
