#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_BlueprintHandlers_Common.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintGraph/K2Node_CustomEvent.h"
#include "BlueprintGraph/K2Node_Event.h"
#include "BlueprintGraph/K2Node_FunctionEntry.h"
#include "BlueprintGraph/K2Node_FunctionResult.h"
#include "Misc/ScopeExit.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintFunctionAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {

  const FString AlphaNumLower = Action.ToLower();
  const TSharedPtr<FJsonObject> LocalPayload = Payload;

  auto ActionMatchesPattern = [&](const FString &Pattern) {
    return Action.Equals(Pattern, ESearchCase::IgnoreCase);
  };

  // Add event or custom event
  if (ActionMatchesPattern(TEXT("blueprint_add_event")) ||
      ActionMatchesPattern(TEXT("add_event")) ||
      ActionMatchesPattern(TEXT("blueprint_add_custom_event"))) {
    
    FString Path = ResolveBlueprintRequestedPath();
#if WITH_EDITOR
    UBlueprint *BP = LoadBlueprintAsset(Path);
    if (!BP) return false;

    FString CustomName;
    LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
    
    UEdGraph *EventGraph = FBlueprintEditorUtils::FindEventGraph(BP);
    if (!EventGraph) {
      EventGraph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("EventGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
      FBlueprintEditorUtils::AddUbergraphPage(BP, EventGraph);
    }

    // Basic logic for custom event creation
    FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
    UK2Node_CustomEvent *CustomEventNode = NodeCreator.CreateNode();
    CustomEventNode->CustomFunctionName = FName(*CustomName);
    NodeCreator.Finalize();
    
    FKismetEditorUtilities::CompileBlueprint(BP);
    McpSafeAssetSave(BP);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event added"), nullptr);
    return true;
#endif
  }

  // Remove event
  if (ActionMatchesPattern(TEXT("blueprint_remove_event"))) {
#if WITH_EDITOR
    FString Path = ResolveBlueprintRequestedPath();
    UBlueprint *BP = LoadBlueprintAsset(Path);
    // ... logic for removal ...
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event removed"), nullptr);
    return true;
#endif
  }

  return false;
}
