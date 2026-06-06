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

#if WITH_EDITOR
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

  FString AssetName = FPaths::GetBaseFilename(PackagePath);

  // Method 1: FindObject with full object path (fastest for in-memory)
  if (UBlueprint* BP = FindObject<UBlueprint>(nullptr, *ObjectPath)) {
    OutNormalized = PackagePath;
    return BP;
  }

  // Method 2: Find package first, then find asset within it
  if (UPackage* Package = FindPackage(nullptr, *PackagePath)) {
    if (UBlueprint* BP = FindObject<UBlueprint>(Package, *AssetName)) {
      OutNormalized = PackagePath;
      return BP;
    }
  }

  // Method 3: TObjectIterator fallback - iterate all blueprints to find by path
  // This is slower but guaranteed to find in-memory assets that weren't properly registered
  for (TObjectIterator<UBlueprint> It; It; ++It) {
    UBlueprint* BP = *It;
    if (BP) {
      FString BPPath = BP->GetPathName();
      // Match by full object path or package path
      if (BPPath.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
          BPPath.Equals(PackagePath, ESearchCase::IgnoreCase) ||
          BPPath.Equals(Path, ESearchCase::IgnoreCase) ||
          BPPath.Equals(Req, ESearchCase::IgnoreCase)) {
        OutNormalized = PackagePath;
        return BP;
      }
      // Also check if the package paths match
      FString BPPackagePath = BPPath;
      if (BPPackagePath.Contains(TEXT("."))) {
        BPPackagePath = BPPackagePath.Left(BPPackagePath.Find(TEXT(".")));
      }
      if (BPPackagePath.Equals(PackagePath, ESearchCase::IgnoreCase)) {
        OutNormalized = PackagePath;
        return BP;
      }
    }
  }

  // Method 4: UEditorAssetLibrary existence check + LoadObject
  if (UEditorAssetLibrary::DoesAssetExist(ObjectPath)) {
    if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ObjectPath)) {
      OutNormalized = PackagePath;
      return BP;
    }
  }

  // Method 5: Asset Registry lookup
  FAssetRegistryModule &ARM =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  FAssetData Found;
  TArray<FAssetData> Results;
  ARM.Get().GetAssetsByPackageName(FName(*PackagePath), Results);
  if (Results.Num() > 0) {
    Found = Results[0];
  }

  if (Found.IsValid()) {
    UBlueprint* BP = Cast<UBlueprint>(Found.GetAsset());
    if (!BP) {
      const FString PathStr = Found.ToSoftObjectPath().ToString();
      BP = LoadObject<UBlueprint>(nullptr, *PathStr);
    }
    if (BP) {
      OutNormalized = Found.ToSoftObjectPath().ToString();
      if (OutNormalized.Contains(TEXT(".")))
        OutNormalized = OutNormalized.Left(OutNormalized.Find(TEXT(".")));
      return BP;
    }
  }

  OutError = FString::Printf(TEXT("Blueprint asset not found: %s"), *Req);
  return nullptr;
}
#endif
