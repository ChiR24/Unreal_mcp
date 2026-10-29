#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Domains/Blueprint/Components/McpBPScsParentResolve.h"

#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

namespace McpBlueprintHandlers {
namespace {
void ApplyModifyScsAttachComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString AttachComponentName;
Op->TryGetStringField(TEXT("componentName"), AttachComponentName);
FString ParentName;
Op->TryGetStringField(TEXT("parentComponent"), ParentName);
if (ParentName.IsEmpty())
  Op->TryGetStringField(TEXT("attachTo"), ParentName);
// edit:"reparent" names its target newParent; the same word works here.
if (ParentName.IsEmpty())
  Op->TryGetStringField(TEXT("newParent"), ParentName);
// The subsystem route matched names against the EXPORTED TEXT of opaque
// subobject handles, so it never found either end and always fell back to a
// bare AddChildNode that also left the node where it was: nested twice. The
// shared resolver detaches first and reaches inherited parents too.
OpSummary->SetStringField(TEXT("componentName"), AttachComponentName);
if (ParentName.TrimStartAndEnd().IsEmpty()) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), TEXT("attach needs parentComponent (or attachTo)"));
  return;
}
OpSummary->SetBoolField(TEXT("success"), true);
McpScsParent::AttachAndReport(LocalBP, LocalSCS, AttachComponentName, ParentName, OpSummary);
}
}

void ApplyModifyScsOperation(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const FString &NormalizedType, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
  if (NormalizedType == TEXT("modify_component") || NormalizedType == TEXT("add_component")) {
    ApplyModifyScsComponentOperation(LocalBP, LocalSCS, NormalizedType, Op, OpSummary);
  } else if (NormalizedType == TEXT("remove_component")) {
    // modify_scs is a write: removing a component is the destructive remove_scs_component, which asks the destructive
    // scope and consent this batch never checked.
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(TEXT("warning"), TEXT("remove_component is not a modify_scs operation: remove a component "
                                                    "with manage_blueprint remove_scs_component (destructive, with consent)"));
  } else if (NormalizedType == TEXT("attach_component") || NormalizedType == TEXT("reparent")) {
    ApplyModifyScsAttachComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(TEXT("warning"), FString::Printf(
        TEXT("Unknown operation type '%s'; use add_component, modify_component "
             "or attach_component (or reparent)"),
        *NormalizedType));
  }
}
} // namespace McpBlueprintHandlers
