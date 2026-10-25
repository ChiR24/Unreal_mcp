#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/BlueprintGraph/McpBPGraphCompatibility.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {

// A variable node carries its pins from the property it names. SetSelfMember
// on a name the Blueprint does not own resolves to nothing, so
// AllocateDefaultPins produces a node with NO pins at all -- and the call
// still answered success. The failure only surfaced one round trip later, as
// "No target pin matched ... ToNode 'K2Node_VariableGet_1' pins: ." from a
// connect_pins that looked like the real problem.
//
// memberClass now does what a caller reading another object's property
// expects: the node becomes an EXTERNAL member of that class, which gives it
// a target pin to wire a cast result into (a class this Blueprint is itself
// keeps a self member, as create_node does). Without it, only the Blueprint's
// own and inherited variables resolve.
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

  // The same lookup, self-or-target choice and access check as create_node, so
  // the two edits give one variable the same node.
  const FName VarFName(*VariableName);
  UClass *Owner = nullptr;
  bool bOwnVariable = false;
  FProperty *Property = McpBlueprintGraphHandlers::ResolveVariableMember(
      BP, VarFName, MemberClass, Owner, bOwnVariable);
  if (!Property && !bOwnVariable) {
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
  const FString Access = McpBlueprintGraphHandlers::DescribeVariableAccess(
      BP, Property, bIsSetter);
  if (!Access.IsEmpty()) {
    OutErrorResult = McpHandlerUtils::CreateResultObject();
    OutErrorResult->SetStringField(TEXT("error"), Access);
    OutErrorMessage = Access;
    OutErrorCode = TEXT("VARIABLE_NOT_ACCESSIBLE");
    return nullptr;
  }

  UK2Node_Variable *VarNode =
      bIsSetter
          ? static_cast<UK2Node_Variable *>(
                NewObject<UK2Node_VariableSet>(TargetGraph))
          : static_cast<UK2Node_Variable *>(
                NewObject<UK2Node_VariableGet>(TargetGraph));
  if (VarNode) {
    McpBlueprintGraphHandlers::SetVariableReference(
        *VarNode, BP, Property, Owner, bOwnVariable, VarFName);
  }
  return VarNode;
}

} // namespace McpBlueprintHandlers
