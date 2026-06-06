#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersBlueprintPaths.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintRemoveEvent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_remove_event")) ||
      ActionMatchesPattern(TEXT("remove_event")) ||
      AlphaNumLower.Contains(TEXT("blueprintremoveevent")) ||
      AlphaNumLower.Contains(TEXT("removeevent"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_remove_event requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }
    FString EventName;
    LocalPayload->TryGetStringField(TEXT("eventName"), EventName);
    if (EventName.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("eventName required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString NormPath;
    const FString RegistryPath =
        (FindBlueprintNormalizedPath(Path, NormPath) && !NormPath.IsEmpty())
            ? NormPath
            : Path;

    // CRITICAL FIX: Validate that the blueprint exists BEFORE treating operation as idempotent.
    // Previously, the code returned success for non-existent blueprints, causing false negatives
    // in tests that expect "not found" errors for invalid paths.
    bool bBlueprintExists = false;
#if WITH_EDITOR
    FString NormalizedCheck;
    FString CheckLoadErr;
    UBlueprint *CheckBlueprint = LoadBlueprintAsset(RegistryPath, NormalizedCheck, CheckLoadErr);
    bBlueprintExists = (CheckBlueprint != nullptr);
#endif
    if (!bBlueprintExists) {
      // Check if path exists in asset registry as fallback
      bBlueprintExists = FindBlueprintNormalizedPath(RegistryPath, NormPath);
    }

    TSharedPtr<FJsonObject> Entry =
        FMcpAutomationBridge_EnsureBlueprintEntry(RegistryPath);
    TArray<TSharedPtr<FJsonValue>> Events =
        Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events"))
                                        : TArray<TSharedPtr<FJsonValue>>();
    int32 FoundIdx = INDEX_NONE;
    for (int32 i = 0; i < Events.Num(); ++i) {
      const TSharedPtr<FJsonValue> &V = Events[i];
      if (!V.IsValid() || V->Type != EJson::Object)
        continue;
      const TSharedPtr<FJsonObject> Obj = V->AsObject();
      FString CandidateName;
      if (Obj->TryGetStringField(TEXT("name"), CandidateName) &&
          CandidateName.Equals(EventName, ESearchCase::IgnoreCase)) {
        FoundIdx = i;
        break;
      }
    }
    if (FoundIdx == INDEX_NONE) {
      // FIX: If blueprint doesn't exist, return error instead of idempotent success.
      // Tests expect "not found" for non-existent blueprint paths.
      if (!bBlueprintExists) {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("eventName"), EventName);
        Resp->SetStringField(TEXT("blueprintPath"), Path);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Blueprint not found."),
                               Resp, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
      }
      // Treat remove as idempotent: if the event is not present in
      // the registry AND blueprint exists, consider the request successful (no-op).
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("blueprintPath"), Path);
      Resp->SetStringField(
          TEXT("note"),
          TEXT("Event not present; treated as removed (idempotent)."));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Event not present; treated as removed"),
                             Resp, FString());
      // Fire completion event to satisfy waitForEvent clients
      TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
      Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
      Notify->SetStringField(TEXT("event"), TEXT("remove_event_completed"));
      Notify->SetStringField(TEXT("requestId"), RequestId);
      Notify->SetObjectField(TEXT("result"), Resp);
      if (Bridge.ConnectionManager.IsValid()) {
        Bridge.ConnectionManager->SendControlMessage(Notify);
      }
      return true;
    }

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
    FString NormalizedRemove;
    FString RemoveLoadErr;
    UBlueprint *RemoveBlueprint =
        LoadBlueprintAsset(RegistryPath, NormalizedRemove, RemoveLoadErr);
    if (RemoveBlueprint) {
      if (UEdGraph *RemoveGraph =
              FBlueprintEditorUtils::FindEventGraph(RemoveBlueprint)) {
        RemoveGraph->Modify();
        TArray<UEdGraphNode *> NodesToRemove;
        for (UEdGraphNode *Node : RemoveGraph->Nodes) {
          if (UK2Node_CustomEvent *CustomEvent =
                  Cast<UK2Node_CustomEvent>(Node)) {
            if (CustomEvent->CustomFunctionName.ToString().Equals(
                    EventName, ESearchCase::IgnoreCase)) {
              NodesToRemove.Add(CustomEvent);
            }
          }
        }
        for (UEdGraphNode *Node : NodesToRemove) {
          RemoveGraph->RemoveNode(Node);
        }
        if (NodesToRemove.Num() > 0) {
          FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
              RemoveBlueprint);
          McpSafeCompileBlueprint(RemoveBlueprint);
          SaveLoadedAssetThrottled(RemoveBlueprint);
        }
      }
    }
#endif // WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
       // Update registry
    Events.RemoveAt(FoundIdx);
    Entry->SetArrayField(TEXT("events"), Events);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("eventName"), EventName);
    Resp->SetStringField(TEXT("blueprintPath"), RegistryPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Event removed."), Resp, FString());
    // Broadcast completion event so clients waiting for an automation_event can
    // resolve
    TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
    Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
    Notify->SetStringField(TEXT("event"), TEXT("remove_event_completed"));
    Notify->SetStringField(TEXT("requestId"), RequestId);
    Notify->SetObjectField(TEXT("result"), Resp);
    if (Bridge.ConnectionManager.IsValid()) {
      Bridge.ConnectionManager->SendControlMessage(Notify);
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: event '%s' removed from '%s'"),
           *EventName, *RegistryPath);
    return true;
  }

  // Add a function to the blueprint (synchronous editor implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
