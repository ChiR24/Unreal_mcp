#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersScsLookup.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersSubobjectTraits.h"

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
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
bool bAttached = false;
USubobjectDataSubsystem *Subsystem = nullptr;
if (GEngine)
  Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
if (Subsystem) {
  TArray<FSubobjectDataHandle> Handles;
  Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, Handles);
  FSubobjectDataHandle ChildHandle, ParentHandle;
  const UScriptStruct *HandleStruct =
      FSubobjectDataHandle::StaticStruct();
  for (const FSubobjectDataHandle &H : Handles) {
    if (!HandleStruct)
      continue;
    FString HText;
    HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None,
                             nullptr);
    if (!AttachComponentName.IsEmpty() &&
        HText.Contains(AttachComponentName, ESearchCase::IgnoreCase))
      ChildHandle = H;
    if (!ParentName.IsEmpty() &&
        HText.Contains(ParentName, ESearchCase::IgnoreCase))
      ParentHandle = H;
  }
  constexpr bool bHasAttach =
      McpAutomationBridge::THasAttach<USubobjectDataSubsystem>::value;
  if (ChildHandle.IsValid() && ParentHandle.IsValid()) {
    if constexpr (bHasAttach) {
      bAttached = Subsystem->AttachSubobject(ParentHandle, ChildHandle);
    }
  }
}
if (bAttached) {
  OpSummary->SetBoolField(TEXT("success"), true);
  OpSummary->SetStringField(TEXT("componentName"), AttachComponentName);
  OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
} else {
  USCS_Node *ChildNode =
      FindScsNodeByName(LocalSCS, AttachComponentName);
  USCS_Node *ParentNode = FindScsNodeByName(LocalSCS, ParentName);
  if (ChildNode && ParentNode) {
    ParentNode->AddChildNode(ChildNode);
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"),
                              AttachComponentName);
    OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(
        TEXT("warning"),
        TEXT("Attach failed: child or parent not found"));
  }
}
#else
USCS_Node *ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName);
USCS_Node *ParentNode = FindScsNodeByName(LocalSCS, ParentName);
if (ChildNode && ParentNode) {
  ParentNode->AddChildNode(ChildNode);
  OpSummary->SetBoolField(TEXT("success"), true);
  OpSummary->SetStringField(TEXT("componentName"), AttachComponentName);
  OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
} else {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"),
      TEXT("Attach failed: child or parent not found"));
}
#endif
}
}

void ApplyModifyScsOperation(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const FString &NormalizedType, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
  if (NormalizedType == TEXT("modify_component") || NormalizedType == TEXT("add_component")) {
    ApplyModifyScsComponentOperation(LocalBP, LocalSCS, NormalizedType, Op, OpSummary);
  } else if (NormalizedType == TEXT("remove_component")) {
    ApplyModifyScsRemoveComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else if (NormalizedType == TEXT("attach_component")) {
    ApplyModifyScsAttachComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else {
    OpSummary->SetBoolField(TEXT("success"), false);
    OpSummary->SetStringField(TEXT("warning"), TEXT("Unknown operation type"));
  }
}
#endif
} // namespace McpBlueprintHandlers
