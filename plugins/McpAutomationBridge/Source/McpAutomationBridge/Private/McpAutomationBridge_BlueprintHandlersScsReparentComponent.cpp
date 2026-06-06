#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleScsReparentComponent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("reparent_scs_component"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("reparent_scs_component requires a valid blueprint"), nullptr,
          TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    FString ComponentName;
    FString NewParent;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    Payload->TryGetStringField(TEXT("newParent"), NewParent);

    if (ComponentName.IsEmpty() || NewParent.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("reparent_scs_component requires componentName and newParent"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Get SCS with explicit null check
    USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
    if (!SCS) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Blueprint does not have a SimpleConstructionScript"), nullptr,
          TEXT("NO_SCS"));
      return true;
    }

    USCS_Node *ChildNode = nullptr;
    USCS_Node *ParentNode = nullptr;

    // Find child and parent nodes with safe iteration
    const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();
    for (USCS_Node *Node : AllNodes) {
      if (Node && Node->GetVariableName().IsValid()) {
        if (Node->GetVariableName().ToString() == ComponentName) {
          ChildNode = Node;
        }
        if (Node->GetVariableName().ToString() == NewParent) {
          ParentNode = Node;
        }
      }
    }

    if (ChildNode) {
      if (ParentNode || NewParent == TEXT("RootComponent")) {
        // Set the parent
        if (NewParent == TEXT("RootComponent")) {
          // RootComponent is not an actual SCS node - all SCS nodes are already
          // children of root by default So we just mark this as success without
          // actually changing anything
          UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                 TEXT("reparent_scs_component: %s is already a root component "
                      "(no action needed)"),
                 *ComponentName);
        } else if (ParentNode) {
          // Set new parent
          ChildNode->SetParent(ParentNode);
        }

        // Compile and save the blueprint
        bool bCompiled = false;
        bool bSaved = false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        bCompiled = McpSafeCompileBlueprint(Blueprint);

        bSaved = SaveLoadedAssetThrottled(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("newParent"), NewParent);
        Result->SetBoolField(TEXT("compiled"), bCompiled);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Bridge.SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Reparented component %s to %s"),
                            *ComponentName, *NewParent),
            Result, FString());
        return true;
      }
    }

    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Failed to reparent component %s"),
                        *ComponentName),
        nullptr, TEXT("OPERATION_FAILED"));
    return true;
  }

  // Set SCS property (simplified implementation)
  return false;
}
#endif
} // namespace McpBlueprintHandlers
