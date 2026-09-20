#pragma once

// Real IK-Rig retargeting. Assigning a target skeleton to a duplicated
// AnimSequence is not a retarget: the tracks keep their source bone names, so
// a Mixamo take dropped onto the UE5 mannequin resolves nothing and plays as
// the bind pose while the call reports success. These helpers build the two
// IK Rigs and the retargeter UE actually needs, then bake through them.

#include "CoreMinimal.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#if __has_include("RigEditor/IKRigController.h")
#define MCP_HAS_IKRIG_PIPELINE 1
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigAutoCharacterizer.h"
#include "RigEditor/IKRigController.h"
#include "RigEditor/IKRigDefinitionFactory.h"
#else
#define MCP_HAS_IKRIG_PIPELINE 0
#endif

#if MCP_HAS_IKRIG_PIPELINE

// An IK Rig is built from a mesh, not a skeleton: the retarget needs the
// reference pose and the limb proportions, and a USkeleton carries neither.
inline USkeletalMesh *McpFindMeshForSkeleton(USkeleton *Skeleton) {
  if (Skeleton == nullptr) {
    return nullptr;
  }
  if (USkeletalMesh *Preview = Skeleton->GetPreviewMesh()) {
    return Preview;
  }
  IAssetRegistry &Registry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
          .Get();
  FARFilter Filter;
  Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
  Filter.bRecursivePaths = true;
  TArray<FAssetData> Meshes;
  Registry.GetAssets(Filter, Meshes);
  const FString SkeletonPath = Skeleton->GetPathName();
  for (const FAssetData &Mesh : Meshes) {
    const FString Tag =
        Mesh.GetTagValueRef<FString>(FName(TEXT("Skeleton")));
    if (Tag.Contains(SkeletonPath)) {
      return Cast<USkeletalMesh>(Mesh.GetAsset());
    }
  }
  return nullptr;
}

// Auto-characterization is what makes this usable without a human: it matches
// the hierarchy against UE's shipped templates (UE5 mannequin, Mixamo, Daz and
// friends) and lays in the retarget chains and root that a person would
// otherwise click in one at a time.
inline UIKRigDefinition *McpBuildIKRig(USkeletalMesh *Mesh,
                                       const FString &PackagePath,
                                       const FString &AssetName,
                                       FString &OutError) {
  if (Mesh == nullptr) {
    OutError = TEXT("No skeletal mesh to build an IK Rig from");
    return nullptr;
  }
  // CreateNewIKRigAsset uniquifies, so re-running a retarget would leave
  // IKR_Foo, IKR_Foo1, IKR_Foo2 behind and none of them the one in use.
  UIKRigDefinition *Rig =
      LoadObject<UIKRigDefinition>(nullptr, *(PackagePath / AssetName));
  if (Rig == nullptr) {
    Rig = UIKRigDefinitionFactory::CreateNewIKRigAsset(PackagePath, AssetName);
  }
  if (Rig == nullptr) {
    OutError = FString::Printf(TEXT("Could not create IK Rig %s/%s"),
                               *PackagePath, *AssetName);
    return nullptr;
  }
  UIKRigController *Controller = UIKRigController::GetController(Rig);
  if (Controller == nullptr) {
    OutError = TEXT("IK Rig has no controller");
    return nullptr;
  }
  Controller->SetSkeletalMesh(Mesh);
  FAutoCharacterizeResults Results;
  Controller->AutoGenerateRetargetDefinition(Results);
  Controller->SetRetargetDefinition(Results.AutoRetargetDefinition.RetargetDefinition);
  Rig->MarkPackageDirty();
  return Rig;
}

inline UIKRetargeter *McpBuildRetargeter(UIKRigDefinition *SourceRig,
                                         UIKRigDefinition *TargetRig,
                                         const FString &PackagePath,
                                         const FString &AssetName,
                                         FString &OutError) {
  UPackage *Package = CreatePackage(*(PackagePath / AssetName));
  if (Package == nullptr) {
    OutError = FString::Printf(TEXT("Could not create package %s/%s"),
                               *PackagePath, *AssetName);
    return nullptr;
  }
  UIKRetargeter *Retargeter = NewObject<UIKRetargeter>(
      Package, UIKRetargeter::StaticClass(), FName(*AssetName),
      RF_Public | RF_Standalone | RF_Transactional);
  UIKRetargeterController *Controller =
      UIKRetargeterController::GetController(Retargeter);
  if (Controller == nullptr) {
    OutError = TEXT("IK Retargeter has no controller");
    return nullptr;
  }
  Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
  Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
  // Exact first so identically named chains bind to their twin, then fuzzy for
  // the rest: two rigs characterized from different templates agree on most
  // chain names but not all, and an unmapped chain silently drops that limb.
  Controller->AutoMapChains(EAutoMapChainType::Exact, /*bForceRemap=*/true);
  Controller->AutoMapChains(EAutoMapChainType::Fuzzy, /*bForceRemap=*/false);
  FAssetRegistryModule::AssetCreated(Retargeter);
  Retargeter->MarkPackageDirty();
  return Retargeter;
}

#endif // MCP_HAS_IKRIG_PIPELINE
