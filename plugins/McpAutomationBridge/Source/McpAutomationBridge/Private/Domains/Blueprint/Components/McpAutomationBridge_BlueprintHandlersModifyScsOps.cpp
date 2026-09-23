#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersScsLookup.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersSubobjectTraits.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsParentResolve.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
namespace {
void ApplyModifyScsRemoveComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
bool bRemoved = false;
USubobjectDataSubsystem *Subsystem = nullptr;
if (GEngine)
  Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
if (Subsystem) {
  TArray<FSubobjectDataHandle> ExistingHandles;
  Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP,
                                                ExistingHandles);
  FSubobjectDataHandle FoundHandle;
  bool bFound = false;
  const UScriptStruct *HandleStruct =
      FSubobjectDataHandle::StaticStruct();
  for (const FSubobjectDataHandle &H : ExistingHandles) {
    if (!HandleStruct)
      continue;
    FString HText;
    HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None,
                             nullptr);
    if (HText.Contains(ComponentName, ESearchCase::IgnoreCase)) {
      FoundHandle = H;
      bFound = true;
      break;
    }
  }

  if (bFound) {
    constexpr bool bHasDelete =
        McpAutomationBridge::THasDeleteSubobject<
            USubobjectDataSubsystem>::value;
    if constexpr (bHasDelete) {
      FSubobjectDataHandle ContextHandle =
          ExistingHandles.Num() > 0 ? ExistingHandles[0] : FoundHandle;
      Subsystem->DeleteSubobject(ContextHandle, FoundHandle, LocalBP);
      bRemoved = true;
    }
  }
}
if (bRemoved) {
  OpSummary->SetBoolField(TEXT("success"), true);
  OpSummary->SetStringField(TEXT("componentName"), ComponentName);
} else {
  if (USCS_Node *TargetNode =
          FindScsNodeByName(LocalSCS, ComponentName)) {
    LocalSCS->RemoveNode(TargetNode);
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(
        TEXT("warning"), TEXT("Component not found; remove skipped"));
  }
}
#else
if (USCS_Node *TargetNode =
        FindScsNodeByName(LocalSCS, ComponentName)) {
  LocalSCS->RemoveNode(TargetNode);
  OpSummary->SetBoolField(TEXT("success"), true);
  OpSummary->SetStringField(TEXT("componentName"), ComponentName);
} else {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"), TEXT("Component not found; remove skipped"));
}
#endif
}

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
    ApplyModifyScsRemoveComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else if (NormalizedType == TEXT("attach_component") || NormalizedType == TEXT("reparent")) {
    ApplyModifyScsAttachComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(TEXT("warning"), FString::Printf(
        TEXT("Unknown operation type '%s'; use add_component, modify_component, "
             "attach_component (or reparent) or remove_component"),
        *NormalizedType));
  }
}
#endif
} // namespace McpBlueprintHandlers
