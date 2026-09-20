#pragma once

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AutomatedAssetImportData.h"
#include "CoreMinimal.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

// An FBX carrying a mocap take imports as a bare SkeletalMesh and the animation
// is silently dropped. AssetTools routes an automated import through Interchange
// whenever no factory is named, so naming UFbxFactory is what lets any of these
// options reach the importer at all. Returns nullptr when the caller asked for
// none of this, which leaves the default Interchange path untouched.
inline UFactory *McpMakeFbxAnimationFactory(UAutomatedAssetImportData *Owner,
                                            bool bImportAnimations,
                                            const FString &SkeletonPath,
                                            FString &OutError,
                                            FString &OutErrorCode) {
  // Naming a skeleton has no meaning except to import a take against it, so it
  // implies animation import rather than quietly importing nothing. The TS door
  // infers the same thing; the plugin re-derives it because it is the authority.
  if (!bImportAnimations && SkeletonPath.IsEmpty()) {
    return nullptr;
  }
  USkeleton *Skel = SkeletonPath.IsEmpty()
                        ? nullptr
                        : LoadObject<USkeleton>(nullptr, *SkeletonPath);
  // Falling back to a mesh import here would answer a request to retarget a
  // take onto a named rig with an unrelated asset, and call it success.
  if (Skel == nullptr && !SkeletonPath.IsEmpty()) {
    OutError = FString::Printf(TEXT("No Skeleton at %s"), *SkeletonPath);
    OutErrorCode = TEXT("SKELETON_NOT_FOUND");
    return nullptr;
  }
  UFbxFactory *Fbx = NewObject<UFbxFactory>(Owner);
  Fbx->ImportUI->bImportAnimations = true;
  Fbx->ImportUI->bAutomatedImportShouldDetectType = false;
  // A skeleton means "import the take alone against this rig", which is what
  // retargeting a downloaded clip onto an existing character needs.
  Fbx->ImportUI->bImportMesh = Skel == nullptr;
  Fbx->ImportUI->MeshTypeToImport =
      Skel != nullptr ? FBXIT_Animation : FBXIT_SkeletalMesh;
  Fbx->ImportUI->Skeleton = Skel;
  // Mocap rarely lands on a whole frame, and the importer rejects a take that
  // does not, with nobody here to answer the prompt it would otherwise raise.
  Fbx->ImportUI->AnimSequenceImportData->bSnapToClosestFrameBoundary = true;
  return Fbx;
}

// UFbxFactory::FactoryCreateFile hands an already-loaded destination object to
// FReimportManager and returns early, which throws away the options above: the
// same call then yields a different result depending on whether the asset
// happens to be resident in memory, and it is resident exactly when a previous
// import in this session put it there. Clearing the object first makes the
// caller's options decide the import instead of the editor's memory state.
// Returns false and fills OutError when something is in the way and the caller
// did not ask to overwrite it.
inline bool McpClearFbxImportTarget(const FString &DestPath,
                                    const FString &DestName, bool bOverwrite,
                                    FString &OutError,
                                    FString &OutErrorCode) {
  const FSoftObjectPath Target(DestPath / DestName + TEXT(".") + DestName);
  // Ask the registry, not memory: whether the asset happens to be loaded is
  // exactly the accident that made this call non-deterministic to begin with.
  IAssetRegistry &Registry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
          .Get();
  if (!Registry.GetAssetByObjectPath(Target, false).IsValid()) {
    return true;
  }
  if (!bOverwrite) {
    OutError = FString::Printf(
        TEXT("%s/%s already exists and would be reimported with its own stored ")
        TEXT("settings, dropping the import options given here. Pass ")
        TEXT("overwrite:true to replace it, or choose an empty destination."),
        *DestPath, *DestName);
    OutErrorCode = TEXT("DESTINATION_OCCUPIED");
    return false;
  }
  UObject *Existing = Target.TryLoad();
  if (Existing == nullptr ||
      !ObjectTools::DeleteSingleObject(Existing, /*bPerformReferenceCheck=*/false)) {
    OutError = FString::Printf(TEXT("Could not replace the existing %s/%s"),
                               *DestPath, *DestName);
    OutErrorCode = TEXT("REPLACE_FAILED");
    return false;
  }
  return true;
}
