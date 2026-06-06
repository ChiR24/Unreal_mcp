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
bool HandleScsRemoveComponent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_SCS_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("remove_scs_component"))) {
    UBlueprint *Blueprint = ResolveBlueprint();
    if (!Blueprint) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("remove_scs_component requires a valid blueprint"), nullptr,
          TEXT("INVALID_BLUEPRINT"));
      return true;
    }

    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);

    if (ComponentName.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("remove_scs_component requires componentName"), nullptr,
          TEXT("INVALID_ARGUMENT"));
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

    // Find and remove the SCS node
    const TArray<USCS_Node *> &AllNodes = SCS->GetAllNodes();
    for (USCS_Node *Node : AllNodes) {
      if (Node && Node->GetVariableName().IsValid() &&
          Node->GetVariableName().ToString() == ComponentName) {
        SCS->RemoveNode(Node);

        // Compile and save the blueprint
        bool bCompiled = false;
        bool bSaved = false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        bCompiled = McpSafeCompileBlueprint(Blueprint);

        bSaved = SaveLoadedAssetThrottled(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("compiled"), bCompiled);
        Result->SetBoolField(TEXT("saved"), bSaved);
        Bridge.SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Removed component %s from SCS"),
                            *ComponentName),
            Result, FString());
        return true;
      }
    }

    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Component %s not found in SCS"), *ComponentName),
        nullptr, TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Get SCS hierarchy
  return false;
}
#endif
} // namespace McpBlueprintHandlers
