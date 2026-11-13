#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintGraph/McpBPGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool McpBlueprintAddEventCustom(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &FinalType, const FString &CustomName,
    const TArray<TSharedPtr<FJsonValue>> &Params) {
  UEngineWireSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
  // A type no pin takes became a wildcard parameter, a nameless entry was skipped, and the event answered success.
  const FString BadParam = McpBlueprintUtils::DescribeInvalidParam(Params, TEXT("parameters"));
  if (!BadParam.IsEmpty()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId, BadParam, TEXT("TYPE_RESOLUTION_FAILED"));
    return true;
  }

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
  }

  // Handle parameters for custom events. A parameter the node already has is skipped: an event
  // added again gained each one a second time under a suffixed name.
  if (CustomEventNode && Params.Num() > 0) {
    CustomEventNode->Modify();
    // A custom event's parameters are output pins on the node.
    FEngineWire_AddUserPins(CustomEventNode, Params, EGPD_Output, /*bSkipExisting=*/true);

    CustomEventNode->ReconstructNode();
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  McpSafeCompileBlueprint(BP);
  const bool bSaved = SaveLoadedAssetThrottled(BP);

  SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                              RegistryKey, EventName, FinalType, Params, bSaved);
  return true;
}
} // namespace McpBlueprintHandlers
