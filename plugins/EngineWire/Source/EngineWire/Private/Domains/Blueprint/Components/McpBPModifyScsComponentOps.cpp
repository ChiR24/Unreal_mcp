#include "Domains/Blueprint/McpBPActionContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Domains/Blueprint/Components/McpBPScsOpFields.h"
#include "Domains/Blueprint/Components/McpBPScsTemplateAssets.h"
#include "Domains/SCS/McpSCSParentResolve.h"
#include "Domains/SCS/McpSCSPropagate.h"
#include "Domains/Blueprint/Components/McpBPScsPropBag.h"
#include "Domains/Blueprint/Components/McpBPScsTransform.h"
#include "Foundation/BridgeHelpers/Blueprints/McpPropCdoComponents.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SubobjectDataSubsystem.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintHandlers {
namespace {
void ApplyModifyScsModifyComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
const TSharedPtr<FJsonValue> PropertiesVal = Op->TryGetField(TEXT("properties"));
const TSharedPtr<FJsonObject> PropertiesObj = PropertiesVal.IsValid() && PropertiesVal->Type == EJson::Object ? PropertiesVal->AsObject() : nullptr;
if (ComponentName.IsEmpty()) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"), TEXT("Missing component name"));
  return;
}
USCS_Node *Node = FindScsNodeByName(LocalSCS, ComponentName);
// An inherited native component (ACharacter's Mesh, CapsuleComponent, ...) has
// no USCS_Node, so the SCS-only lookup reported "Component not found" for a
// component the Blueprint plainly has. set_scs_property has resolved those
// through the CDO for a while; the batch path had not.
UActorComponent *Template = Node ? Node->ComponentTemplate : nullptr;
if (!Template) { Template = McpScsParent::FindInheritedComponent(LocalBP, ComponentName); }
// A parent Blueprint's component is edited through this Blueprint's override of it, as the Details panel does: get_scs
// listed it as inherited while every edit here called it unknown. An override nothing landed on is undone below.
McpPropertyCdoComponents::FCreatedInheritedOverride Created;
if (!Template && LocalBP && LocalBP->GeneratedClass) {
  Template = McpPropertyCdoComponents::FindCdoComponent(LocalBP, LocalBP->GeneratedClass->GetDefaultObject(), ComponentName, true, &Created);
}
if (!Template) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), FString::Printf(
      TEXT("'%s' is neither a component of this Blueprint nor one it inherits; call get_scs to list what it has."), *ComponentName));
  return;
}
bool bAnySuccess = false;
McpScsPropagate::FDefaults Defaults{Template, LocalBP, FName(*ComponentName)};
// What did not land - transform parts, properties, mesh and material alike - is named in the batch's warnings.
TArray<FString> Rejected = McpScsTransform::Apply(Template, Op, Defaults, bAnySuccess);
if (PropertiesObj.IsValid()) {
  const TArray<FString> Refused = McpScsPropertyBag::Apply(Template, PropertiesObj, Defaults, bAnySuccess);
  // The names that took their value ("Name: reason" is how a refused one reads), as add_scs_component answers them.
  TArray<FString> Applied;
  for (const auto &Pair : PropertiesObj->Values) {
    const FString Key(*Pair.Key); // 5.8 keys are not FString
    if (!Refused.ContainsByPredicate([&Key](const FString &Each) { return Each.StartsWith(Key + TEXT(":")); })) {
      Applied.Add(Key);
    }
  }
  OpSummary->SetArrayField(TEXT("appliedProperties"), McpHandlerUtils::ToJsonStringArray(Applied));
  Rejected.Append(Refused);
}
for (const TCHAR *Path : {TEXT("StaticMesh"), TEXT("OverrideMaterials")}) Defaults.Capture(Path);
bool bAssetsApplied = false;
Rejected.Append(ApplyScsTemplateAssets(Template, Op, bAssetsApplied));
bAnySuccess = bAssetsApplied || bAnySuccess;
if (Rejected.Num() > 0) {
  TArray<TSharedPtr<FJsonValue>> Values = McpHandlerUtils::ToJsonStringArray(Rejected);
  OpSummary->SetArrayField(TEXT("rejectedProperties"), Values);
}
const FString HiddenHint = bAssetsApplied ? McpScsHiddenHint(Template, ComponentName) : FString();
if (!HiddenHint.IsEmpty()) { OpSummary->SetStringField(TEXT("hint"), HiddenHint); }
McpScsPropagate::PropagateAndReport(Defaults, OpSummary);
McpScsPropagate::Pending().Emplace(Template, Defaults);
OpSummary->SetBoolField(TEXT("success"), bAnySuccess);
OpSummary->SetStringField(TEXT("componentName"), ComponentName);
if (!bAnySuccess) {
  Created.Rollback();
  OpSummary->SetStringField(TEXT("warning"), TEXT("No transform, properties, mesh or material applied"));
}
}

void ApplyModifyScsAddComponent(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
FString ComponentName;
Op->TryGetStringField(TEXT("componentName"), ComponentName);
const FString ComponentClassPath = ScsOpComponentClass(Op);
FString AttachToName;
Op->TryGetStringField(TEXT("attachTo"), AttachToName);
// Each add compiles the Blueprint, which replaces its SCS - so by op 2 the
// pointer handed in at the start of the batch is stale and every lookup misses.
if (LocalBP && LocalBP->SimpleConstructionScript) {
  LocalSCS = LocalBP->SimpleConstructionScript;
}

// UE 5.7 FIX: Use ResolveClassByName to handle short class names like "StaticMeshComponent"
// FSoftClassPath triggers ensure failure when given short package names in UE 5.7+
// ResolveClassByName handles short name resolution via prefix guessing and TObjectIterator
UClass *ComponentClass = ResolveClassByName(ComponentClassPath);

// Fallback: Only use FSoftClassPath if it looks like a full path (contains /)
if (!ComponentClass && ComponentClassPath.Contains(TEXT("/"))) {
  FSoftClassPath ComponentClassSoftPath(ComponentClassPath);
  ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
}
if (!ComponentClass) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), ComponentClassPath.IsEmpty()
      ? FString(TEXT("add_component needs a componentClass (or componentType)"))
      : FString::Printf(TEXT("Component class not found: '%s'"), *ComponentClassPath));
// The test the single add makes, before either way of adding: the fallback below is a check() for a class that is
// no component, which took the editor down for componentClass "Actor".
} else if (const FString ClassRefusal = McpScsComponentClassRefusal(ComponentClass, ComponentClassPath); !ClassRefusal.IsEmpty()) {
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(TEXT("warning"), ClassRefusal);
} else {
  USCS_Node *ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
  if (ExistingNode) {
    OpSummary->SetBoolField(TEXT("success"), true);
    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
    OpSummary->SetStringField(TEXT("warning"),
                              TEXT("Component already exists"));
    // Re-running a prefab definition is how a caller corrects one, so an
    // existing node takes this call's transform, properties, mesh and material
    // instead of being skipped outright.
    if (ScsOpHasEdits(Op)) {
      ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Op, OpSummary);
    }
  } else {
    bool bAddedViaSubsystem = false;
    USubobjectDataSubsystem *Subsystem = nullptr;
    if (GEngine)
      Subsystem =
          GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
    if (Subsystem) {
      TArray<FSubobjectDataHandle> ExistingHandles;
      Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP,
                                                    ExistingHandles);
      // Added under the first handle, the root: a subobject handle carries no name to pick another parent by, so
      // the attach below moves the new component under the parent the operation names.
      FAddNewSubobjectParams Params;
      Params.ParentHandle = ExistingHandles.Num() > 0 ? ExistingHandles[0] : FSubobjectDataHandle();
      Params.NewClass = ComponentClass;
      Params.BlueprintContext = LocalBP;
      FText FailReason;
      const FSubobjectDataHandle NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
      if (NewHandle.IsValid()) {
        // A member of that name, or of the name with the _GEN_VARIABLE suffix, takes the first free <name>_<n>.
        FString UniqueName = ComponentName;
        FName TargetVarName = FName(*UniqueName);
        if (const UClass *Class = LocalBP->GeneratedClass) {
          if (Class->FindPropertyByName(TargetVarName) ||
              Class->FindPropertyByName(FName(*(UniqueName + TEXT("_GEN_VARIABLE"))))) {
            for (int32 Suffix = 1; Suffix < 1000; ++Suffix) {
              UniqueName = FString::Printf(TEXT("%s_%d"), *ComponentName, Suffix);
              TargetVarName = FName(*UniqueName);
              if (!Class->FindPropertyByName(TargetVarName)) {
                break;
              }
            }
            OpSummary->SetStringField(TEXT("originalName"), ComponentName);
            OpSummary->SetStringField(TEXT("renamedTo"), UniqueName);
          }
        }
        Subsystem->RenameSubobjectMemberVariable(LocalBP, NewHandle, TargetVarName);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
        // Compiled for the operations after it; saving is the batch's own step (save, applyAndSave): a save here
        // wrote the Blueprint to disk under save:false while the reply said saved:false.
        McpSafeCompileBlueprint(LocalBP);
        bAddedViaSubsystem = true;
      }
    }
    if (bAddedViaSubsystem) {
      OpSummary->SetBoolField(TEXT("success"), true);
      OpSummary->SetStringField(TEXT("componentName"), ComponentName);
      // The node was added under the root: attach it by the name it got now that it exists, and report it. Looked
      // up by the name asked for, a renamed node was never found and stayed on the root.
      FString AddedName = ComponentName;
      OpSummary->TryGetStringField(TEXT("renamedTo"), AddedName);
      McpScsParent::AttachAndReport(LocalBP, LocalBP->SimpleConstructionScript,
                                    AddedName, AttachToName, OpSummary);
      if (AttachToName.TrimStartAndEnd().IsEmpty()) {
        McpScsParent::ReportParent(LocalBP->SimpleConstructionScript, AddedName, OpSummary);
      }
      // The contract says add_component also takes `transform`, a `properties`
      // bag, `meshPath` and `materialPath`, but this only ever created the node:
      // in a batch every placement and every mesh assignment was accepted,
      // reported success, and silently dropped, so a prefab built this way came
      // out as a pile of empty components at the origin. Reuse the modify path
      // on the node just added, whichever of them the operation names.
      if (ScsOpHasEdits(Op)) {
        TSharedPtr<FJsonObject> Applied = MakeShared<FJsonObject>(*Op);
        Applied->SetStringField(TEXT("componentName"), AddedName);
        ApplyModifyScsModifyComponent(LocalBP, LocalBP->SimpleConstructionScript, Applied, OpSummary);
      }
    } else {
      USCS_Node *NewNode =
          LocalSCS->CreateNode(ComponentClass, *ComponentName);
      if (NewNode) {
        LocalSCS->AddNode(NewNode);
        OpSummary->SetBoolField(TEXT("success"), true);
        OpSummary->SetStringField(TEXT("componentName"), ComponentName);
        McpScsParent::AttachAndReport(LocalBP, LocalSCS, ComponentName,
                                      AttachToName, OpSummary);
        if (AttachToName.TrimStartAndEnd().IsEmpty()) {
          McpScsParent::ReportParent(LocalSCS, ComponentName, OpSummary);
        }
        // The subsystem route above places and configures what it added; this one dropped both.
        if (ScsOpHasEdits(Op)) {
          ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Op, OpSummary);
        }
      } else {
        OpSummary->SetBoolField(TEXT("success"), false);
        OpSummary->SetStringField(TEXT("warning"),
                                  TEXT("Failed to create SCS node"));
      }
    }
  }
}
}
} // namespace

void ApplyModifyScsComponentOperation(UBlueprint *LocalBP, USimpleConstructionScript *LocalSCS, const FString &NormalizedType, const TSharedPtr<FJsonObject> &Op, TSharedPtr<FJsonObject> OpSummary) {
  // location/rotation/scale on the operation itself take the road `transform` takes, for add and modify alike.
  McpScsTransform::FoldDirect(Op);
  if (NormalizedType == TEXT("modify_component")) {
    ApplyModifyScsModifyComponent(LocalBP, LocalSCS, Op, OpSummary);
  } else if (NormalizedType == TEXT("add_component")) {
    ApplyModifyScsAddComponent(LocalBP, LocalSCS, Op, OpSummary);
  }
}
} // namespace McpBlueprintHandlers
