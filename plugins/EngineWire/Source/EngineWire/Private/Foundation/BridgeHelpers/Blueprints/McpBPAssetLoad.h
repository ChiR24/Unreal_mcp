#pragma once

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoreMinimal.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

// Attempt to locate and load a Blueprint by several heuristics. Returns nullptr
/**
 * Locate and load a Blueprint asset from a variety of request formats and
 * return the loaded Blueprint.
 *
 * Attempts to resolve the input `Req` as an exact asset path (package.object),
 * a package path (with /Game/ prepended when missing), or by querying the Asset
 * Registry for a matching package name. On success `OutNormalized` is set to a
 * normalized package path (without the object suffix) and the loaded
 * `UBlueprint*` is returned; on failure `OutError` is set and nullptr is
 * returned.
 *
 * @param Req The requested asset identifier; may be an absolute package path,
 * an object-qualified path (Package.Asset), or a short path relative to /Game
 * (e.g., "Folder/Asset" or "/Game/Folder/Asset").
 * @param OutNormalized Out parameter that will receive the normalized package
 * path for the resolved asset (no object suffix) on success.
 * @param OutError Out parameter that will receive a descriptive error message
 * if resolution or loading fails.
 * @returns The loaded `UBlueprint*` when the asset is found and loaded, or
 * `nullptr` on failure.
 */
static inline UBlueprint *LoadBlueprintAsset(const FString &Req,
                                             FString &OutNormalized,
                                             FString &OutError) {
  OutNormalized.Empty();
  OutError.Empty();
  if (Req.IsEmpty()) {
    OutError = TEXT("Empty request");
    return nullptr;
  }

  // Build normalized paths
  FString Path = Req;
  if (!Path.StartsWith(TEXT("/"))) {
    Path = TEXT("/Game/") + Path;
  }

  FString ObjectPath = Path;
  FString PackagePath = Path;

  if (Path.Contains(TEXT("."))) {
    PackagePath = Path.Left(Path.Find(TEXT(".")));
  } else {
    FString AssetName = FPaths::GetBaseFilename(Path);
    ObjectPath = Path + TEXT(".") + AssetName;
  }

  // In memory first: by object path, then any Blueprint in that package (a
  // just-created one the registry has not indexed yet).
  UBlueprint* InMemory = FindObject<UBlueprint>(nullptr, *ObjectPath);
  if (!InMemory) {
    if (UPackage* Package = FindPackage(nullptr, *PackagePath)) {
      InMemory = Cast<UBlueprint>(static_cast<UObject*>(
          FindObjectWithOuter(Package, UBlueprint::StaticClass())));
    }
  }
  if (InMemory) {
    OutNormalized = PackagePath;
    return InMemory;
  }

  // From disk: Asset Registry lookup. (A UEditorAssetLibrary::DoesAssetExist
  // step used to precede it; that refuses and logs an engine error in PIE.)
  FAssetRegistryModule &ARM =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  TArray<FAssetData> Results;
  ARM.Get().GetAssetsByPackageName(FName(*PackagePath), Results);
  // GetAsset loads the asset when it is not in memory yet.
  if (UBlueprint* BP = Results.Num() > 0 ? Cast<UBlueprint>(Results[0].GetAsset()) : nullptr) {
    OutNormalized = Results[0].PackageName.ToString();
    return BP;
  }

  OutError = FString::Printf(TEXT("Blueprint asset not found: %s"), *Req);
  return nullptr;
}
