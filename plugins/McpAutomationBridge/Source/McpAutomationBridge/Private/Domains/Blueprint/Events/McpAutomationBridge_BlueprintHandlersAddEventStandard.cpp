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
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
bool McpBlueprintAddEventStandard(
    const FBlueprintActionContext &Context, UBlueprint *BP, UEdGraph *EventGraph,
    int32 EventPosX, int32 EventPosY, const FString &RegistryKey,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params) {
  UMcpAutomationBridgeSubsystem &Bridge = Context.Bridge;
  const FString &RequestId = Context.RequestId;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;

  FString TargetEventName = FinalType;
  static TMap<FString, FString> EventNameAliases = {
      {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
      {TEXT("Tick"), TEXT("ReceiveTick")},
      {TEXT("EndPlay"), TEXT("ReceiveEndPlay")},
  };

  if (const FString *Alias = EventNameAliases.Find(TargetEventName)) {
    TargetEventName = *Alias;
  }

  FName EventName = FName(*TargetEventName);

  UClass *TargetClass = nullptr;
  UFunction *EventFunc = nullptr;

  UClass *SearchClass = BP->ParentClass;
  while (SearchClass && !EventFunc) {
    EventFunc = SearchClass->FindFunctionByName(*TargetEventName,
                                                EIncludeSuperFlag::ExcludeSuper);
    if (EventFunc) {
      TargetClass = SearchClass;
      break;
    }
    SearchClass = SearchClass->GetSuperClass();
  }

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
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Event %s already exists, skipping creation (idempotent "
                "success)"),
           *TargetEventName);
    bExists = true;
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
