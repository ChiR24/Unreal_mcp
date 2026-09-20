#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersScsLookup.h"

namespace McpScsParent {

// A Blueprint's own SCS is only half its component tree: everything inherited
// from a native parent class (ACharacter's `Mesh` and `CapsuleComponent`,
// APawn's movement component) lives on the CDO and has no USCS_Node at all.
// Every batch path here used to look only at the SCS, so `attachTo: "Mesh"`
// matched nothing -- and rather than say so, the add path fell back to the
// first handle it had (the root), which is how fourteen body parts silently
// ended up on the collision cylinder while the call reported success.
// Resolving only USceneComponent missed the component most worth editing on a
// character: CharacterMovement is a UActorComponent, so tuning jump height,
// gravity or the plane constraint through the batch path was told the
// component "is neither a component of this Blueprint nor one it inherits" --
// while get_scs listed it, and the refusal told the caller to run get_scs.
inline UActorComponent *FindInheritedComponent(UBlueprint *Blueprint,
                                               const FString &Name) {
  if (!Blueprint || Name.IsEmpty()) {
    return nullptr;
  }
  UClass *Class = Blueprint->GeneratedClass ? Blueprint->GeneratedClass
                                            : Blueprint->ParentClass;
  AActor *CDO = Class ? Cast<AActor>(Class->GetDefaultObject()) : nullptr;
  if (!CDO) {
    return nullptr;
  }
  for (UActorComponent *Component : CDO->GetComponents()) {
    if (!Component) {
      continue;
    }
    // Match the object name AND the UPROPERTY alias: ACharacter's mesh is the
    // object "CharacterMesh0" but every caller, and the editor's own details
    // panel, calls it `Mesh`.
    if (Component->GetName().Equals(Name, ESearchCase::IgnoreCase)) {
      return Component;
    }
    if (FProperty *Property = Class->FindPropertyByName(FName(*Name))) {
      if (FObjectProperty *ObjectProp = CastField<FObjectProperty>(Property)) {
        if (ObjectProp->GetObjectPropertyValue_InContainer(CDO) == Component) {
          return Component;
        }
      }
    }
  }
  return nullptr;
}

// Attach targets must still be scene components; this keeps one lookup.
inline USceneComponent *FindInheritedSceneComponent(UBlueprint *Blueprint,
                                                    const FString &Name) {
  return Cast<USceneComponent>(FindInheritedComponent(Blueprint, Name));
}

// Attaches Child under ParentName, which may name an SCS node in this
// Blueprint or an inherited/native scene component. Returns false with a
// caller-facing reason rather than quietly leaving the node where it was.
inline bool AttachNodeToNamedParent(UBlueprint *Blueprint,
                                    USimpleConstructionScript *SCS,
                                    USCS_Node *Child, const FString &ParentName,
                                    FString &OutResolvedAs, FString &OutError) {
  if (!SCS || !Child) {
    OutError = TEXT("No SCS or component node to attach.");
    return false;
  }
  const FString Trimmed = ParentName.TrimStartAndEnd();
  if (Trimmed.IsEmpty()) {
    OutError = TEXT("No parent name given.");
    return false;
  }

  if (USCS_Node *ParentNode = FindScsNodeByName(SCS, Trimmed)) {
    if (ParentNode == Child) {
      OutError = TEXT("A component cannot be its own parent.");
      return false;
    }
    if (USCS_Node *OldParent = SCS->FindParentNode(Child)) {
      OldParent->RemoveChildNode(Child, /*bRemoveFromAllNodes=*/false);
    }
    ParentNode->AddChildNode(Child);
    OutResolvedAs = TEXT("scs");
    return true;
  }

  if (USceneComponent *Native = FindInheritedSceneComponent(Blueprint, Trimmed)) {
    // A node parented to a native component is a ROOT node of the SCS that
    // carries the parent's name plus bIsParentComponentNative; SetParent sets
    // both. Detach from any SCS parent first so it is not in two places.
    if (USCS_Node *OldParent = SCS->FindParentNode(Child)) {
      OldParent->RemoveChildNode(Child, /*bRemoveFromAllNodes=*/false);
      SCS->AddNode(Child);
    }
    Child->SetParent(Native);
    OutResolvedAs = TEXT("inherited");
    return true;
  }

  OutError = FString::Printf(
      TEXT("Parent '%s' is neither a component of this Blueprint nor one it "
           "inherits. Inherited components are addressable by their property "
           "name (Mesh, CapsuleComponent, CharacterMovement); call get_scs to "
           "list what this Blueprint actually has."),
      *Trimmed);
  return false;
}

// One-line call site for the batch ops: attach by name and say in the op
// summary what happened. An `attachTo` that cannot be resolved is reported as
// a failed op instead of being dropped on the floor, which is what let a whole
// prefab land on the root while every op claimed success.
inline void AttachAndReport(UBlueprint *Blueprint,
                            USimpleConstructionScript *SCS,
                            const FString &ChildName, const FString &ParentName,
                            const TSharedPtr<FJsonObject> &OpSummary) {
  if (ParentName.TrimStartAndEnd().IsEmpty() || !OpSummary.IsValid()) {
    return;
  }
  USCS_Node *Child = FindScsNodeByName(SCS, ChildName);
  FString ResolvedAs;
  FString Error;
  if (Child &&
      AttachNodeToNamedParent(Blueprint, SCS, Child, ParentName, ResolvedAs, Error)) {
    OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
    OpSummary->SetStringField(TEXT("attachedToKind"), ResolvedAs);
    return;
  }
  OpSummary->SetBoolField(TEXT("success"), false);
  OpSummary->SetStringField(
      TEXT("warning"),
      Child ? Error
            : FString::Printf(TEXT("Added component '%s' could not be found to "
                                   "attach it to '%s'."),
                              *ChildName, *ParentName));
}

}  // namespace McpScsParent
#endif
