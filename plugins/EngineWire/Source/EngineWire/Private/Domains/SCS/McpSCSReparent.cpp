#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Blueprint/Components/McpBPScsParentResolve.h"

#include "Domains/SCS/McpSCSHandlers.h"
#include "Domains/SCS/McpSCSSupport.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject>
FSCSHandlers::ReparentSCSComponent(const FString &BlueprintPath,
                                   const FString &ComponentName,
                                   const FString &NewParentNameAsSent) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  // Trimmed once: ' Body ' missed the finder, was attached by AttachNodeToNamedParent (which trims) as a node of this
  // SCS, and the reply said SCS_PARENT_NOT_FOUND after the node had moved.
  const FString NewParentName = NewParentNameAsSent.TrimStartAndEnd();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result);
  if (!Blueprint) {
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *ComponentNode = FindScsNodeByName(SCS, ComponentName);

  if (!ComponentNode) {
    return SCSFail(Result, FString::Printf(TEXT("Component not found: %s"), *ComponentName), TEXT("SCS_COMPONENT_NOT_FOUND"));
  }

  USCS_Node *NewParentNode = nullptr;
  if (IsSCSRootAlias(SCS, NewParentName)) {
    // The engine's root: null when native or a parent Blueprint's, whose children sit at the top of the SCS, and the
    // top too when the component is that root.
    NewParentNode = SCSRootAliasNode(SCS);
    NewParentNode = NewParentNode == ComponentNode ? nullptr : NewParentNode;
  } else if (!NewParentName.IsEmpty()) {
    NewParentNode = FindScsNodeByName(SCS, NewParentName);

    // An inherited native component (ACharacter's Mesh, CapsuleComponent, ...)
    // has no USCS_Node, so reparenting onto one answered SCS_PARENT_NOT_FOUND
    // even though the editor does it with a drag. SetParent records the native
    // parent on the node itself, which is how the editor stores it too.
    if (!NewParentNode) {
      FString ResolvedAs;
      FString AttachError;
      if (McpScsParent::AttachNodeToNamedParent(Blueprint, SCS, ComponentNode,
                                                NewParentName, ResolvedAs,
                                                AttachError) &&
          ResolvedAs == TEXT("inherited")) {
        // Every other exit from this handler runs FinalizeBlueprintSCSChange;
        // this one only marked the Blueprint modified, so a reparent onto an
        // inherited component was reported as success but never compiled or
        // saved and did not survive a restart.
        bool bInheritedCompiled = false;
        bool bInheritedSaved = false;
        FinalizeBlueprintSCSChange(Blueprint, bInheritedCompiled, bInheritedSaved);
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("message"),
                               FString::Printf(TEXT("Reparented %s under inherited component %s"),
                                               *ComponentName, *NewParentName));
        Result->SetStringField(TEXT("parentKind"), TEXT("inherited"));
        Result->SetBoolField(TEXT("compiled"), bInheritedCompiled);
        Result->SetBoolField(TEXT("saved"), bInheritedSaved);
        AddSCSNodeVerification(Result, SCS, ComponentNode);
        return Result;
      }
      SCSFail(Result, AttachError, TEXT("SCS_PARENT_NOT_FOUND"));
      AddSCSNodeVerification(Result, SCS, ComponentNode);
      return Result;
    }
  }

  if (NewParentNode == ComponentNode) {
    SCSFail(Result, TEXT("Cannot reparent a component to itself"), TEXT("SCS_SELF_REPARENT"));
    AddSCSNodeVerification(Result, SCS, ComponentNode);
    return Result;
  }

  const FString ExpectedParentName =
      NewParentNode ? GetSCSNodeName(NewParentNode) : FString();
  const FString ParentDisplayName =
      ExpectedParentName.IsEmpty() ? FString(TEXT("(root)")) : ExpectedParentName;

  USCS_Node *OldParent = SCS->FindParentNode(ComponentNode);

  // A node hanging on an inherited part is in the root set too, its parent recorded on the node: sent to the root it
  // answered no change and stayed on that part.
  if ((NewParentNode == nullptr && OldParent == nullptr &&
       IsSCSRootNode(SCS, ComponentNode) && ComponentNode->ParentComponentOrVariableName.IsNone()) ||
      (OldParent != nullptr && NewParentNode == OldParent)) {
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(
        TEXT("message"),
        TEXT("Component already under requested parent; no changes made"));
    AddSCSNodeVerification(Result, SCS, ComponentNode);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    return Result;
  }

  if (NewParentNode && NewParentNode->IsChildOf(ComponentNode)) {
    return SCSFail(Result, TEXT("Cannot create circular parent-child relationship"), TEXT("CYCLIC_PARENT"));
  }

  if (OldParent) {
    OldParent->RemoveChildNode(ComponentNode);
  } else {
    // Also what clears a root node's inherited-parent fields; for a node neither a root nor a child it does nothing.
    SCS->RemoveNode(ComponentNode);
  }

  if (NewParentNode) {
    NewParentNode->AddChildNode(ComponentNode);
  } else {
    SCS->AddNode(ComponentNode);
  }

  FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  USCS_Node *VerifiedNode = FindScsNodeByName(SCS, ComponentName);
  if (!VerifiedNode ||
      !SCSParentMatches(SCS, VerifiedNode, ExpectedParentName)) {
    SCSFail(Result,
            FString::Printf(TEXT("Verification failed: Component '%s' was not reparented to '%s'"),
                            *ComponentName, *ParentDisplayName),
            TEXT("SCS_REPARENT_VERIFICATION_FAILED"));
    if (VerifiedNode) {
      AddSCSNodeVerification(Result, SCS, VerifiedNode);
    }
    Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetBoolField(TEXT("saved"), bSaved);
    return Result;
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' reparented to '%s'"), *ComponentName,
                      *ParentDisplayName));
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  AddSCSNodeVerification(Result, SCS, VerifiedNode);
  McpHandlerUtils::AddVerification(Result, Blueprint);

  return Result;
}
