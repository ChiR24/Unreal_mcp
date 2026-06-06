#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_SCSHandlers.h"
#include "McpAutomationBridge_SCSHandlersSupport.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#endif

using namespace McpSCSHandlers;

TSharedPtr<FJsonObject> FSCSHandlers::AddSCSComponent(
    const FString &BlueprintPath, const FString &ComponentClass,
    const FString &ComponentName, const FString &ParentComponentName,
    const FString &MeshPath, const FString &MaterialPath) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

#if WITH_EDITOR
  if (IsPlayInEditorActive()) {
    return PIEActiveError();
  }

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

  USimpleConstructionScript *SCS = Blueprint->SimpleConstructionScript;
  if (!SCS) {
    SCS = NewObject<USimpleConstructionScript>(Blueprint);
    Blueprint->SimpleConstructionScript = SCS;
  }

  UClass *CompClass = ResolveClassByName(ComponentClass);
  if (!CompClass) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"), FString::Printf(TEXT("Component class not found: %s"),
                                       *ComponentClass));
    return Result;
  }

  if (!CompClass->IsChildOf(UActorComponent::StaticClass())) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Class is not a component: %s"), *ComponentClass));
    return Result;
  }

  USCS_Node *ParentNode = nullptr;
  if (!ParentComponentName.IsEmpty()) {
    if (IsSCSRootAlias(ParentComponentName)) {
      const TArray<USCS_Node *> &Roots = SCS->GetRootNodes();
      for (USCS_Node *Root : Roots) {
        if (Root && GetSCSNodeName(Root).Equals(
                        TEXT("DefaultSceneRoot"), ESearchCase::IgnoreCase)) {
          ParentNode = Root;
          break;
        }
      }
      if (!ParentNode && !Roots.IsEmpty()) {
        ParentNode = Roots[0];
      }
    } else {
      ParentNode = FindSCSNodeByVariableName(SCS, ParentComponentName);
      if (!ParentNode) {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(
            TEXT("error"),
            FString::Printf(TEXT("Parent component not found: %s"),
                            *ParentComponentName));
        return Result;
      }
    }
  }

  for (USCS_Node *Node : SCS->GetAllNodes()) {
    if (Node && Node->GetVariableName().IsValid() &&
        Node->GetVariableName().ToString().Equals(ComponentName,
                                                  ESearchCase::IgnoreCase)) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("Component with name '%s' already exists"),
                          *ComponentName));
      return Result;
    }
  }

  USCS_Node *NewNode = SCS->CreateNode(CompClass, FName(*ComponentName));
  if (!NewNode) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("Failed to create SCS node"));
    return Result;
  }

  NewNode->SetVariableName(FName(*ComponentName));

  if (ParentNode) {
    ParentNode->AddChildNode(NewNode);
  } else {
    SCS->AddNode(NewNode);
  }

  bool bMeshApplied = false;
  if (!MeshPath.IsEmpty() && NewNode->ComponentTemplate) {
    if (UStaticMeshComponent *SMC =
            Cast<UStaticMeshComponent>(NewNode->ComponentTemplate)) {
      UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
      if (Mesh) {
        SMC->SetStaticMesh(Mesh);
        bMeshApplied = true;
      }
    } else if (USkeletalMeshComponent *SkMC =
                   Cast<USkeletalMeshComponent>(NewNode->ComponentTemplate)) {
      USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
      if (Mesh) {
        SkMC->SetSkeletalMesh(Mesh, true);
        bMeshApplied = true;
      }
    }
  }

  bool bMaterialApplied = false;
  if (!MaterialPath.IsEmpty() && NewNode->ComponentTemplate) {
    if (UPrimitiveComponent *PC =
            Cast<UPrimitiveComponent>(NewNode->ComponentTemplate)) {
      UMaterialInterface *Mat =
          LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
      if (Mat) {
        PC->SetMaterial(0, Mat);
        bMaterialApplied = true;
      }
    }
  }

  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  USCS_Node *VerifiedNode = FindSCSNodeByVariableName(SCS, ComponentName);
  const bool bVerified = VerifiedNode != nullptr;
  const bool bParentVerified =
      bVerified && SCSParentMatches(SCS, VerifiedNode, ParentComponentName);

  if (!bVerified || !bParentVerified) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"),
                           bVerified
                               ? FString::Printf(
                                     TEXT("Verification failed: Component '%s' "
                                          "parent did not match requested parent '%s'"),
                                     *ComponentName, *ParentComponentName)
                               : FString::Printf(
                                     TEXT("Verification failed: Component '%s' "
                                          "not found in SCS after add"),
                                     *ComponentName));
    Result->SetStringField(TEXT("errorCode"), TEXT("SCS_VERIFICATION_FAILED"));
    if (VerifiedNode) {
      AddSCSNodeVerification(Result, SCS, VerifiedNode);
    }
    return Result;
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' added to SCS"), *ComponentName));
  Result->SetStringField(TEXT("component_name"), ComponentName);
  Result->SetStringField(TEXT("component_class"), CompClass->GetName());
  Result->SetStringField(TEXT("parent"), ParentComponentName.IsEmpty()
                                             ? TEXT("(root)")
                                             : ParentComponentName);
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  AddSCSNodeVerification(Result, SCS, VerifiedNode);
  Result->SetBoolField(TEXT("mesh_applied"), bMeshApplied);
  Result->SetBoolField(TEXT("material_applied"), bMaterialApplied);
  McpHandlerUtils::AddVerification(Result, Blueprint);
  if (NewNode && NewNode->ComponentTemplate) {
    if (USceneComponent *SceneComp =
            Cast<USceneComponent>(NewNode->ComponentTemplate)) {
      AddComponentVerification(Result, SceneComp);
    }
  }
#else
  return UnsupportedSCSAction();
#endif

  return Result;
}
