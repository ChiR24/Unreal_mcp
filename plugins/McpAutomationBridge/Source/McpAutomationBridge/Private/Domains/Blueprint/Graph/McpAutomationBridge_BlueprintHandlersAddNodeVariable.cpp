#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2

// A variable node carries its pins from the property it names. SetSelfMember
// on a name the Blueprint does not own resolves to nothing, so
// AllocateDefaultPins produces a node with NO pins at all -- and the call
// still answered success. The failure only surfaced one round trip later, as
// "No target pin matched ... ToNode 'K2Node_VariableGet_1' pins: ." from a
// connect_pins that looked like the real problem. Refuse up front and say
// which Blueprint was searched, since the usual cause is naming a variable
// that lives on some OTHER class (memberClass is not honoured here -- read
// another object's property through a cast plus a getter on the cast result).
UEdGraphNode *MakeVariableNodeForMcp(UBlueprint *BP, UEdGraph *TargetGraph,
                                     const FString &NodeTypeLower,
                                     const FString &VariableName,
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

  if (!FMcpAutomationBridge_FindProperty(BP, VariableName)) {
    OutErrorResult = McpHandlerUtils::CreateResultObject();
    OutErrorResult->SetStringField(
        TEXT("error"),
        FString::Printf(
            TEXT("'%s' is not a variable of '%s', so the node would have no "
                 "pins. Add it with add_variable first, or -- if it belongs to "
                 "another class -- cast to that class and wire the cast result "
                 "into a getter's target pin."),
            *VariableName, *BP->GetName()));
    OutErrorMessage = TEXT("Unresolved variable name");
    OutErrorCode = TEXT("VARIABLE_NOT_FOUND");
    return nullptr;
  }

  if (bIsSetter) {
    UK2Node_VariableSet *VarSet = NewObject<UK2Node_VariableSet>(TargetGraph);
    if (VarSet) {
      VarSet->VariableReference.SetSelfMember(FName(*VariableName));
    }
    return VarSet;
  }

  UK2Node_VariableGet *VarGet = NewObject<UK2Node_VariableGet>(TargetGraph);
  if (VarGet) {
    VarGet->VariableReference.SetSelfMember(FName(*VariableName));
  }
  return VarGet;
}

#endif
} // namespace McpBlueprintHandlers
