#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"
#include "McpAutomationBridge_SCSHandlersSupport.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject>
FSCSHandlers::ReparentSCSComponent(const FString &BlueprintPath,
                                   const FString &ComponentName,
                                   const FString &NewParentName) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if WITH_EDITOR
  FString NormalizedPath;
  FString ErrorMsg;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(BlueprintPath, NormalizedPath, ErrorMsg);
  if (!Blueprint) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        ErrorMsg.IsEmpty()
            ? FString::Printf(TEXT("Blueprint asset not found at path: %s"),
                              *BlueprintPath)
            : ErrorMsg);
    return Result;
  }

  if (!Blueprint->SimpleConstructionScript) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Blueprint has no SimpleConstructionScript: %s"),
                        *BlueprintPath));
    return Result;
  }

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;

  USCS_Node *ComponentNode = nullptr;
  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      ComponentNode = Node;
      break;
    }
  }

  if (!ComponentNode) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    return Result;
  }

  USCS_Node *NewParentNode = nullptr;
  if (!NewParentName.IsEmpty()) {
    const bool bRootSynonym =
        NewParentName.Equals(TEXT("RootComponent"), ESearchCase::IgnoreCase) ||
        NewParentName.Equals(TEXT("DefaultSceneRoot"),
                             ESearchCase::IgnoreCase) ||
        NewParentName.Equals(TEXT("Root"), ESearchCase::IgnoreCase);
    if (bRootSynonym) {
      const TArray<USCS_Node *> &Roots = SCS->GetRootNodes();
      for (USCS_Node *R : Roots) {
        if (R && R != ComponentNode && R->GetVariableName().IsValid() &&
            R->GetVariableName().ToString().Equals(TEXT("DefaultSceneRoot"),
                                                   ESearchCase::IgnoreCase)) {
          NewParentNode = R;
          break;
        }
      }
      if (!NewParentNode) {
        for (USCS_Node *R : Roots) {
          if (R && R != ComponentNode) {
            NewParentNode = R;
            break;
          }
        }
      }
    }

    if (!NewParentNode) {
      for (USCS_Node *Node : SCS->GetAllNodes()) {
        if (Node && Node->GetVariableName().IsValid() &&
            Node->GetVariableName().ToString().Equals(
                NewParentName, ESearchCase::IgnoreCase)) {
          NewParentNode = Node;
          break;
        }
      }
    }

    if (!NewParentNode) {
      Result->SetBoolField(TEXT("success"), false);
      const FString ParentError =
          bRootSynonym
              ? FString::Printf(TEXT("Requested root parent alias '%s' could not be resolved to an SCS root node"),
                                *NewParentName)
              : FString::Printf(TEXT("New parent not found: %s"),
                                *NewParentName);
      Result->SetStringField(TEXT("error"), ParentError);
      Result->SetStringField(TEXT("errorCode"), TEXT("SCS_PARENT_NOT_FOUND"));
      AddSCSNodeVerification(Result, SCS, ComponentNode);
      return Result;
    }
  }

  if (NewParentNode == ComponentNode) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           TEXT("Cannot reparent a component to itself"));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_SELF_REPARENT"));
    AddSCSNodeVerification(Result, SCS, ComponentNode);
    return Result;
  }

  const FString ExpectedParentName =
      NewParentNode ? GetSCSNodeName(NewParentNode) : FString();
  const FString ParentDisplayName =
      ExpectedParentName.IsEmpty() ? FString(TEXT("(root)")) : ExpectedParentName;

  auto IsDescendantOf = [](USCS_Node *A, USCS_Node *B) -> bool {
    if (!A || !B) {
      return false;
    }
    TArray<USCS_Node *> Stack;
    Stack.Add(A);
    while (Stack.Num() > 0) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      USCS_Node *Cur = Stack.Pop(EAllowShrinking::No);
#else
      USCS_Node *Cur = Stack.Pop(false);
#endif
      if (!Cur) {
        continue;
      }
      const TArray<USCS_Node *> &Kids = Cur->GetChildNodes();
      for (USCS_Node *K : Kids) {
        if (!K) {
          continue;
        }
        if (K == B) {
          return true;
        }
        Stack.Add(K);
      }
    }
    return false;
  };

  USCS_Node *OldParent = nullptr;
  for (USCS_Node *Candidate : SCS->GetAllNodes()) {
    if (Candidate && Candidate->GetChildNodes().Contains(ComponentNode)) {
      OldParent = Candidate;
      break;
    }
  }

  if ((NewParentNode == nullptr && OldParent == nullptr &&
       IsSCSRootNode(SCS, ComponentNode)) ||
      (OldParent != nullptr && NewParentNode == OldParent)) {
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(
        TEXT("message"),
        TEXT("Component already under requested parent; no changes made"));
    AddSCSNodeVerification(Result, SCS, ComponentNode);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    return Result;
  }

  if (NewParentNode && IsDescendantOf(ComponentNode, NewParentNode)) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        TEXT("Cannot create circular parent-child relationship"));
    return Result;
  }

  if (OldParent) {
    OldParent->RemoveChildNode(ComponentNode);
  } else {
    const bool bReparentingToRoot = (NewParentNode == nullptr);
    if (!bReparentingToRoot) {
      SCS->RemoveNode(ComponentNode);
    }
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

  USCS_Node *VerifiedNode = FindSCSNodeByVariableName(SCS, ComponentName);
  if (!VerifiedNode ||
      !SCSParentMatches(SCS, VerifiedNode, ExpectedParentName)) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Verification failed: Component '%s' was not reparented to '%s'"),
                        *ComponentName, *ParentDisplayName));
    Result->SetStringField(TEXT("errorCode"),
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
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
