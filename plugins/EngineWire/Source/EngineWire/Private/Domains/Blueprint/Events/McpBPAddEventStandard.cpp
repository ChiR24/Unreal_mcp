#include "Domains/Blueprint/McpBPActionContext.h"
#include "K2Node_Event.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool McpBlueprintAddEventStandard(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params) {
  UEngineWireSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;

  // An event of the parent class chain (BeginPlay for ReceiveBeginPlay) or of an interface the Blueprint implements
  // (Interact on BPI_Interactable). A parent function of that name that is no event (K2_DestroyActor) is not one: an
  // override node built for it fails to compile.
  UFunction *EventFunc = McpFindOverridableEvent(BP, FinalType);
  const FString TargetEventName = EventFunc ? EventFunc->GetName() : FinalType;
  const FName EventName = FName(*TargetEventName);
  if (!EventFunc) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not find event '%s' (resolved to '%s') in "
                             "parent class."),
                        *FinalType, *TargetEventName),
        TEXT("EVENT_NOT_FOUND"));
    return true;
  }

  // Check if node already exists.
  bool bExists = false;
  for (UEdGraphNode *Node : EventGraph->Nodes) {
    if (UK2Node_Event *EventNode = Cast<UK2Node_Event>(Node)) {
      if (EventNode->EventReference.GetMemberName() == EventFunc->GetFName()) {
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
    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("Event %s already exists, skipping creation (idempotent "
                "success)"),
           *TargetEventName);
  }

  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
  McpSafeCompileBlueprint(BP);
  const bool bSaved = SaveLoadedAssetThrottled(BP);

  SendBlueprintAddEventResult(Bridge, RequestId, RequestingSocket, BP,
                              RegistryKey, EventName, FinalType, Params, bSaved,
                              EventFunc->GetOwnerClass()->GetPathName());
  return true;
}
} // namespace McpBlueprintHandlers
