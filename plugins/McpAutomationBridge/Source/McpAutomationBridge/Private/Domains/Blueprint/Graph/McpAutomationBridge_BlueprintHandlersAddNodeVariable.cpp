#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2

// A variable node carries its pins from the property it names. SetSelfMember
// on a name the Blueprint does not own resolves to nothing, so
// AllocateDefaultPins produces a node with NO pins at all -- and the call
// still answered success. The failure only surfaced one round trip later, as
// "No target pin matched ... ToNode 'K2Node_VariableGet_1' pins: ." from a
// connect_pins that looked like the real problem.
//
// memberClass now does what a caller reading another object's property
// expects: the node becomes an EXTERNAL member of that class, which gives it
// a target pin to wire a cast result into. Without it, only the Blueprint's
// own variables resolve.
UEdGraphNode *MakeVariableNodeForMcp(UBlueprint *BP, UEdGraph *TargetGraph,
                                     const FString &NodeTypeLower,
                                     const FString &VariableName,
                                     const FString &MemberClass,
                                     FString &OutErrorMessage,
                                     FString &OutErrorCode,
                                     TSharedPtr<FJsonObject> &OutErrorResult) {
  const bool bIsSetter = NodeTypeLower.Contains(TEXT("variableset")) ||
                         NodeTypeLower.Contains(TEXT("setvar"));
  if (VariableName.IsEmpty()) {
    OutErrorResult = McpHandlerUtils::CreateResultObject();
    OutErrorResult->SetStringField(
        TEXT("error"),
        TEXT("A variable node needs memberName naming the variable to read or "
             "write."));
    OutErrorMessage = TEXT("Variable name required");
    OutErrorCode = TEXT("MISSING_VARIABLE_NAME");
    return nullptr;
  }

  const FName VarFName(*VariableName);
  UClass *ExternalOwner = nullptr;
  if (!FMcpAutomationBridge_FindProperty(BP, VariableName)) {
    UClass *Named =
        MemberClass.IsEmpty() ? nullptr : ResolveClassByName(MemberClass);
    if (Named && Named->FindPropertyByName(VarFName)) {
      ExternalOwner = Named;
    } else {
      OutErrorResult = McpHandlerUtils::CreateResultObject();
      OutErrorResult->SetStringField(
          TEXT("error"),
          FString::Printf(
              TEXT("'%s' is not a variable of '%s', so the node would have no "
                   "pins. Add it with add_variable, or name the class that "
                   "owns it in memberClass and wire a cast result into the "
                   "node's target pin."),
              *VariableName, *BP->GetName()));
      OutErrorMessage = TEXT("Unresolved variable name");
      OutErrorCode = TEXT("VARIABLE_NOT_FOUND");
      return nullptr;
    }
  }

  UK2Node_Variable *VarNode =
      bIsSetter
          ? static_cast<UK2Node_Variable *>(
                NewObject<UK2Node_VariableSet>(TargetGraph))
          : static_cast<UK2Node_Variable *>(
                NewObject<UK2Node_VariableGet>(TargetGraph));
  if (VarNode) {
    if (ExternalOwner) {
      VarNode->VariableReference.SetExternalMember(VarFName, ExternalOwner);
    } else {
      VarNode->VariableReference.SetSelfMember(VarFName);
    }
  }
  return VarNode;
}

#endif
} // namespace McpBlueprintHandlers
