#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpHandlerUtils.h"
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

    FString SourceNodeGuid, TargetNodeGuid;
    LocalPayload->TryGetStringField(TEXT("sourceNodeGuid"), SourceNodeGuid);
    LocalPayload->TryGetStringField(TEXT("targetNodeGuid"), TargetNodeGuid);

    if (SourceNodeGuid.IsEmpty() || TargetNodeGuid.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourceNodeGuid and targetNodeGuid required"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SourcePinName, TargetPinName;
    LocalPayload->TryGetStringField(TEXT("sourcePinName"), SourcePinName);
    LocalPayload->TryGetStringField(TEXT("targetPinName"), TargetPinName);

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
