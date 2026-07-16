#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintConnectPins(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_connect_pins")) ||
      ActionMatchesPattern(TEXT("connect_pins")) ||
      AlphaNumLower.Contains(TEXT("blueprintconnectpins"))) {
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_connect_pins requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    // Accept all documented field-name aliases for the source/target node and
    // pin so the TS transport (fromNodeId/fromPinName/toNodeId/toPinName,
    // sourceNode/sourcePin/targetNode/targetPin), the native MCP transport
    // (sourceNodeId/sourcePin/targetNodeId/targetPin), and the linkedTo form
    // all resolve. Canonical sourceNodeGuid/targetNodeGuid remain accepted for
    // backward compatibility. A regression that read only the Guid-prefixed
    // names silently broke every documented connect_pins call on both
    // transports.
    auto PickFirstNonEmpty = [](const TSharedPtr<FJsonObject> &P,
                                const TArray<const TCHAR *> &Keys) -> FString {
      FString V;
      for (const TCHAR *K : Keys) {
        if (P->TryGetStringField(K, V) && !V.TrimStartAndEnd().IsEmpty()) {
          return V;
        }
      }
      return FString();
    };

    FString SourceNodeGuid = PickFirstNonEmpty(
        LocalPayload, {TEXT("sourceNodeGuid"), TEXT("sourceNodeId"),
                       TEXT("fromNodeId"), TEXT("sourceNode"), TEXT("nodeId")});
    FString TargetNodeGuid = PickFirstNonEmpty(
        LocalPayload, {TEXT("targetNodeGuid"), TEXT("targetNodeId"),
                       TEXT("toNodeId"), TEXT("targetNode")});
    FString SourcePinName = PickFirstNonEmpty(
        LocalPayload, {TEXT("sourcePinName"), TEXT("sourcePin"),
                       TEXT("fromPinName"), TEXT("fromPin"), TEXT("outputPin"),
                       TEXT("pinName")});
    FString TargetPinName = PickFirstNonEmpty(
        LocalPayload, {TEXT("targetPinName"), TEXT("targetPin"),
                       TEXT("toPinName"), TEXT("toPin"), TEXT("inputPin")});

    // linkedTo alias form: "TargetNodeId.TargetPinName"
    // Treat linkedTo as a FALLBACK only: populate the target node/pin from it
    // solely when the explicitly-provided target fields were left empty, so a
    // valid targetNodeId/targetPinName (or toNodeId/toPinName) is never
    // clobbered by linkedTo (issue #struct-ecosystem [21]).
    FString LinkedTo;
    if (LocalPayload->TryGetStringField(TEXT("linkedTo"), LinkedTo) &&
        !LinkedTo.IsEmpty()) {
      FString LinkedNode, LinkedPin;
      if (LinkedTo.Split(TEXT("."), &LinkedNode, &LinkedPin)) {
        if (TargetNodeGuid.IsEmpty()) {
          TargetNodeGuid = LinkedNode.TrimStartAndEnd();
        }
        if (TargetPinName.IsEmpty() && !LinkedPin.IsEmpty()) {
          TargetPinName = LinkedPin.TrimStartAndEnd();
        }
      }
    }

    if (SourceNodeGuid.IsEmpty() || TargetNodeGuid.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("connect_pins requires a source and target node identifier "
               "(sourceNodeGuid/fromNodeId/sourceNode/nodeId, or "
               "targetNodeGuid/toNodeId/targetNode/linkedTo)"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

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
    if (!BP) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"), LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr,
                             Result, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    const FString RegistryKey = Normalized.IsEmpty() ? Path : Normalized;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_connect_pins begin Path=%s"),
           *RegistryKey);

    UEdGraphNode *SourceNode = nullptr;
    UEdGraphNode *TargetNode = nullptr;
    FGuid SourceGuid, TargetGuid;
    FGuid::Parse(SourceNodeGuid, SourceGuid);
    FGuid::Parse(TargetNodeGuid, TargetGuid);

    for (UEdGraph *Graph : BP->UbergraphPages) {
      if (!Graph)
        continue;
      for (UEdGraphNode *Node : Graph->Nodes) {
        if (!Node)
          continue;
        if (Node->NodeGuid == SourceGuid)
          SourceNode = Node;
        if (Node->NodeGuid == TargetGuid)
          TargetNode = Node;
      }
    }

    if (!SourceNode || !TargetNode) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(
          TEXT("error"), TEXT("Could not find source or target node by GUID"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Node lookup failed"), Result,
                             TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin *SourcePin = nullptr;
    UEdGraphPin *TargetPin = nullptr;

    auto ResolvePin =
        [](UEdGraphNode *Node, const FString &PreferredName,
           EEdGraphPinDirection DesiredDirection) -> UEdGraphPin * {
      if (!Node)
        return nullptr;
      if (!PreferredName.IsEmpty()) {
        for (UEdGraphPin *Pin : Node->Pins) {
          if (Pin &&
              Pin->GetName().Equals(PreferredName, ESearchCase::IgnoreCase)) {
            return Pin;
          }
        }
      }
      for (UEdGraphPin *Pin : Node->Pins) {
        if (Pin && Pin->Direction == DesiredDirection) {
          return Pin;
        }
      }
      return nullptr;
    };

    SourcePin = ResolvePin(SourceNode, SourcePinName, EGPD_Output);
    TargetPin = ResolvePin(TargetNode, TargetPinName, EGPD_Input);

    if (!SourcePin || !TargetPin) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"),
                             TEXT("Could not find source or target pin"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Pin lookup failed"), Result,
                             TEXT("PIN_NOT_FOUND"));
      return true;
    }

    BP->Modify();
    SourceNode->GetGraph()->Modify();

    const UEdGraphSchema_K2 *Schema =
        Cast<UEdGraphSchema_K2>(SourceNode->GetGraph()->GetSchema());
    bool bSuccess = false;
    if (Schema) {
      bSuccess = Schema->TryCreateConnection(SourcePin, TargetPin);
      if (bSuccess) {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Result->SetStringField(TEXT("sourcePinName"), SourcePin->GetName());
    Result->SetStringField(TEXT("targetPinName"), TargetPin->GetName());

    if (!bSuccess) {
      Result->SetStringField(TEXT("error"),
                             Schema ? TEXT("Schema rejected connection")
                                    : TEXT("Invalid graph schema"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Pin connection failed"), Result,
                             TEXT("CONNECTION_FAILED"));
      return true;
    }

    const bool bSaved = SaveLoadedAssetThrottled(BP);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pin connection complete"), Result, FString());
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Log,
        TEXT("HandleBlueprintAction: blueprint_connect_pins succeeded Path=%s"),
        *RegistryKey);
    return true;
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_connect_pins requires editor build "
                                "with EdGraphSchema_K2"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // blueprint_ensure_exists: Check if blueprint exists, create if not
  return false;
}
#endif
} // namespace McpBlueprintHandlers
