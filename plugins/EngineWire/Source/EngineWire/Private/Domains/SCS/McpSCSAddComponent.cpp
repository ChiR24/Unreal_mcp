#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Blueprint/Components/McpBPScsParentResolve.h"
#include "Domains/SCS/McpSCSHandlers.h"
#include "Domains/SCS/McpSCSSupport.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInterface.h"

using namespace McpSCSHandlers;

namespace {
// An SCS node can legally parent to an inherited native component -- that is what
// USCS_Node::SetParent(const USceneComponent*) is for -- but parent resolution only
// searched SCS nodes, so on a Character every spelling of the inherited capsule and
// mesh was rejected and there was no way to attach a camera, weapon or light to them.
USceneComponent *FindNativeSceneComponent(UBlueprint *BP, const FString &Name) {
  UClass *ParentClass = BP ? BP->ParentClass : nullptr;
  AActor *CDO = ParentClass ? Cast<AActor>(ParentClass->GetDefaultObject()) : nullptr;
  // By object name (CollisionCylinder, CharacterMesh0), else by the property the editor shows (CapsuleComponent, Mesh).
  return CDO ? Cast<USceneComponent>(McpFindActorComponent(CDO, Name, USceneComponent::StaticClass())) : nullptr;
}

// "Parent component not found: X" never said what would have worked. List both
// pools so one round trip is enough.
FString DescribeParentCandidates(UBlueprint *BP, USimpleConstructionScript *SCS) {
  TArray<FString> Names;
  if (SCS) {
    for (USCS_Node *Node : SCS->GetAllNodes()) {
      if (Node) {
        Names.Add(GetSCSNodeName(Node));
      }
    }
  }
  McpPropertyCdoComponents::ForEachScsNode(BP, [&Names](USCS_Node *Node, bool bInherited) {
    if (bInherited && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>()) {
      Names.AddUnique(GetSCSNodeName(Node) + TEXT(" (inherited)"));
    }
    return true;
  });
  UClass *ParentClass = BP ? BP->ParentClass : nullptr;
  if (AActor *CDO = ParentClass ? Cast<AActor>(ParentClass->GetDefaultObject()) : nullptr) {
    for (UActorComponent *Comp : CDO->GetComponents()) {
      if (Cast<USceneComponent>(Comp)) {
        Names.AddUnique(Comp->GetName() + TEXT(" (inherited)"));
      }
    }
  }
  return Names.IsEmpty() ? TEXT("none") : FString::Join(Names, TEXT(", "));
}
} // namespace

TSharedPtr<FJsonObject> FSCSHandlers::AddSCSComponent(
    const FString &BlueprintPath, const FString &ComponentClass,
    const FString &ComponentName, const FString &ParentComponentName,
    const FString &MeshPath, const FString &MaterialPath) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

  UBlueprint *Blueprint = LoadScsBlueprint(BlueprintPath, Result, false);
  if (!Blueprint) {
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

  if (const FString Refusal = McpScsComponentClassRefusal(CompClass, ComponentClass); !Refusal.IsEmpty()) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), Refusal);
    return Result;
  }

  USCS_Node *ParentNode = nullptr;
  USceneComponent *NativeParent = nullptr;
  USCS_Node *InheritedParent = nullptr;
  if (!ParentComponentName.IsEmpty()) {
    if (IsSCSRootAlias(SCS, ParentComponentName)) {
      // The engine's root; null when native or a parent Blueprint's, whose children sit at the top of the SCS.
      ParentNode = SCSRootAliasNode(SCS);
    } else {
      ParentNode = FindScsNodeByName(SCS, ParentComponentName);
      // A parent Blueprint's part, as edit_scs attachTo takes it (this path refused it).
      InheritedParent = ParentNode ? nullptr : McpScsParent::FindInheritedScsSceneNode(Blueprint, ParentComponentName);
      if (!ParentNode && !InheritedParent) {
        NativeParent = FindNativeSceneComponent(Blueprint, ParentComponentName);
      }
      if (!ParentNode && !NativeParent && !InheritedParent) {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(
            TEXT("error"),
            FString::Printf(
                TEXT("Parent component not found: %s. Available: %s"),
                *ParentComponentName,
                *DescribeParentCandidates(Blueprint, SCS)));
        return Result;
      }
      if ((NativeParent || InheritedParent) && !CompClass->IsChildOf(USceneComponent::StaticClass())) {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(
            TEXT("error"),
            FString::Printf(
                TEXT("'%s' is a non-scene component and cannot attach to '%s'"),
                *ComponentName, *ParentComponentName));
        return Result;
      }
    }
  }

  // A name an inherited member holds (a Character's Mesh) too: CreateNode picked a free one and SetVariableName below
  // forced the taken one back, two members of one name.
  if (FindScsNodeByName(SCS, ComponentName) ||
      SCS->GenerateNewComponentName(CompClass, FName(*ComponentName)) != FName(*ComponentName)) {
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(
        TEXT("error"),
        FString::Printf(TEXT("Component with name '%s' already exists"),
                        *ComponentName));
    return Result;
  }
  // A meshPath or materialPath that did not load, or a class that cannot show it, still added the component and
  // answered success with meshApplied false: an empty component the caller took for a mesh. Both refuse first.
  const bool bStaticMeshClass = CompClass->IsChildOf(UStaticMeshComponent::StaticClass());
  const bool bSkeletalMeshClass = CompClass->IsChildOf(USkeletalMeshComponent::StaticClass());
  // Both paths go through the shared canonicalizer and the registry, as every other asset path a request names.
  const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
  const FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
  UObject *MeshAsset = SafeMeshPath.IsEmpty() ? nullptr
      : bStaticMeshClass ? static_cast<UObject *>(Cast<UStaticMesh>(McpLoadAsset(SafeMeshPath)))
      : bSkeletalMeshClass ? static_cast<UObject *>(Cast<USkeletalMesh>(McpLoadAsset(SafeMeshPath))) : nullptr;
  UMaterialInterface *MaterialAsset = !SafeMaterialPath.IsEmpty() && CompClass->IsChildOf(UPrimitiveComponent::StaticClass())
      ? Cast<UMaterialInterface>(McpLoadAsset(SafeMaterialPath)) : nullptr;
  if ((!MeshPath.IsEmpty() && !MeshAsset) || (!MaterialPath.IsEmpty() && !MaterialAsset)) {
    const bool bMesh = !MeshPath.IsEmpty() && !MeshAsset;
    const bool bCanHold = bMesh ? (bStaticMeshClass || bSkeletalMeshClass) : CompClass->IsChildOf(UPrimitiveComponent::StaticClass());
    return SCSFail(Result, bCanHold
        ? FString::Printf(TEXT("%s %s does not load as a %s; nothing was added."), bMesh ? TEXT("meshPath") : TEXT("materialPath"),
              bMesh ? *MeshPath : *MaterialPath, bMesh ? (bStaticMeshClass ? TEXT("static mesh") : TEXT("skeletal mesh")) : TEXT("material"))
        : FString::Printf(TEXT("%s shows no %s; send %s only for a %s. Nothing was added."), *CompClass->GetName(),
              bMesh ? TEXT("mesh") : TEXT("material"), bMesh ? TEXT("meshPath") : TEXT("materialPath"),
              bMesh ? TEXT("StaticMeshComponent or SkeletalMeshComponent") : TEXT("primitive component")),
        bCanHold ? TEXT("ASSET_NOT_FOUND") : TEXT("INVALID_ARGUMENT"));
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
    if (NativeParent) {
      NewNode->SetParent(NativeParent);
    } else if (InheritedParent) {
      NewNode->SetParent(InheritedParent);
    }
  }

  bool bMeshApplied = false;
  if (MeshAsset && NewNode->ComponentTemplate) {
    if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewNode->ComponentTemplate)) {
      SMC->SetStaticMesh(Cast<UStaticMesh>(MeshAsset));
      bMeshApplied = true;
    } else if (USkeletalMeshComponent *SkMC = Cast<USkeletalMeshComponent>(NewNode->ComponentTemplate)) {
      SkMC->SetSkeletalMesh(Cast<USkeletalMesh>(MeshAsset), true);
      bMeshApplied = true;
    }
  }

  bool bMaterialApplied = false;
  if (MaterialAsset && NewNode->ComponentTemplate) {
    if (UPrimitiveComponent *PC = Cast<UPrimitiveComponent>(NewNode->ComponentTemplate)) {
      PC->SetMaterial(0, MaterialAsset);
      bMaterialApplied = true;
    }
  }

  bool bCompiled = false;
  bool bSaved = false;
  FinalizeBlueprintSCSChange(Blueprint, bCompiled, bSaved);

  USCS_Node *VerifiedNode = FindScsNodeByName(SCS, ComponentName);
  const bool bVerified = VerifiedNode != nullptr;
  const bool bParentVerified =
      bVerified && SCSParentMatches(SCS, VerifiedNode, ParentComponentName);

  if (!bVerified || !bParentVerified) {
    SCSFail(Result,
            bVerified ? FString::Printf(TEXT("Verification failed: Component '%s' parent did not match requested parent '%s'"),
                                        *ComponentName, *ParentComponentName)
                      : FString::Printf(TEXT("Verification failed: Component '%s' not found in SCS after add"), *ComponentName),
            TEXT("SCS_VERIFICATION_FAILED"));
    if (VerifiedNode) {
      AddSCSNodeVerification(Result, SCS, VerifiedNode);
    }
    return Result;
  }

  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(
      TEXT("message"),
      FString::Printf(TEXT("Component '%s' added to SCS"), *ComponentName));
  Result->SetStringField(TEXT("componentClass"), CompClass->GetName());
  // Report the parent the engine actually attached to: a new scene component with no
  // explicit parent lands under the root node, not at "(root)".
  FString ActualParent = ParentComponentName;
  if (USCS_Node *ActualParentNode = SCS->FindParentNode(VerifiedNode)) {
    ActualParent = ActualParentNode->GetVariableName().ToString();
  }
  // An actor component (buoyancy, movement) attaches to nothing; scsVerification already says so.
  const bool bSceneComponent = CompClass->IsChildOf(USceneComponent::StaticClass());
  Result->SetStringField(TEXT("parent"), !bSceneComponent ? TEXT("(none - not a scene component)")
                                         : ActualParent.IsEmpty() ? TEXT("(root)") : ActualParent);
  // componentName is the SCS name the caller asked for and can address again; the internal
  // variable/template name (Name_GEN_VARIABLE) rides separately.
  Result->SetStringField(TEXT("componentName"), ComponentName);
  Result->SetStringField(TEXT("variableName"), VerifiedNode->GetVariableName().ToString());
  Result->SetBoolField(TEXT("compiled"), bCompiled);
  Result->SetBoolField(TEXT("saved"), bSaved);
  AddSCSNodeVerification(Result, SCS, VerifiedNode);
  Result->SetBoolField(TEXT("meshApplied"), bMeshApplied);
  Result->SetBoolField(TEXT("materialApplied"), bMaterialApplied);
  McpHandlerUtils::AddVerification(Result, Blueprint);

  return Result;
}
