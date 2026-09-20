#pragma once

// Real IK-Rig retargeting. Assigning a target skeleton to a duplicated
// AnimSequence is not a retarget: the tracks keep their source bone names, so
// a Mixamo take dropped onto the UE5 mannequin resolves nothing and plays as
// the bind pose while the call reports success. These helpers build the two
// IK Rigs and the retargeter UE actually needs, then bake through them.

#include "CoreMinimal.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

// Gated on the NEWEST requirement, not the oldest header. IKRigController.h has
// shipped since 5.0, but the static GetController and SetIKRig(enum) arrived in
// 5.2, the auto-characterizer in 5.4 and CreateNewIKRigAsset in 5.6 (see
// MCP_HAS_IKRIG_CREATE_NEW_ASSET), so a 5.1-5.5 build compiled this and failed.
#if __has_include("RigEditor/IKRigController.h") && \
    __has_include("RigEditor/IKRigAutoCharacterizer.h") && \
    MCP_HAS_IKRIG_CREATE_NEW_ASSET
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
  // Let the registry do the matching. The Skeleton tag is stored either as the
  // plain object path or as export text (Class'/Path'), so both spellings are
  // asked for; the exact compare below then rules out a skeleton whose name
  // merely prefixes another (SK_Manny vs SK_Manny2), which a substring test
  // used to accept. The unfiltered scan is kept only as a fallback for a tag
  // format neither spelling matches.
  const FSoftObjectPath WantedSkeleton(Skeleton);
  const FName SkeletonTag(TEXT("Skeleton"));
  FARFilter Filter;
  Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
  Filter.bRecursivePaths = true;
  Filter.TagsAndValues.Add(SkeletonTag, WantedSkeleton.ToString());
  Filter.TagsAndValues.Add(SkeletonTag, FObjectPropertyBase::GetExportPath(Skeleton));
  TArray<FAssetData> Meshes;
  Registry.GetAssets(Filter, Meshes);
  if (Meshes.Num() == 0) {
    Filter.TagsAndValues.Empty();
    Registry.GetAssets(Filter, Meshes);
  }
  for (const FAssetData &Mesh : Meshes) {
    const FString Tag = Mesh.GetTagValueRef<FString>(SkeletonTag);
    if (!Tag.IsEmpty() && FSoftObjectPath(Tag) == WantedSkeleton) {
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
    Rig = MCP_IKRIG_CREATE_NEW_ASSET(PackagePath, AssetName);
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
  // Saved here so create_ik_rig and setup_retargeting both persist what they
  // report; a rig that only exists in memory is gone with the editor session.
  McpSafeOperations::McpSafeAssetSave(Rig);
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
  MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, SourceRig);
  MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, TargetRig);
  // Exact first so identically named chains bind to their twin, then fuzzy for
  // the rest: two rigs characterized from different templates agree on most
  // chain names but not all, and an unmapped chain silently drops that limb.
  Controller->AutoMapChains(EAutoMapChainType::Exact, /*bForceRemap=*/true);
  Controller->AutoMapChains(EAutoMapChainType::Fuzzy, /*bForceRemap=*/false);
  FAssetRegistryModule::AssetCreated(Retargeter);
  Retargeter->MarkPackageDirty();
  McpSafeOperations::McpSafeAssetSave(Retargeter);
  return Retargeter;
}

#endif // MCP_HAS_IKRIG_PIPELINE
