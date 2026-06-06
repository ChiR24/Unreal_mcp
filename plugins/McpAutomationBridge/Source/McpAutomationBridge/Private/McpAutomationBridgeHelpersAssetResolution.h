#pragma once

// Normalize an asset path to ensure it's in valid long package name format.
// Uses engine FPackageName API for proper validation.
// - If path doesn't start with '/', prepends '/Game/'
// - Removes trailing slashes
// - Returns the normalized path and whether it's valid
// - Reference: Engine/Source/Runtime/CoreUObject/Public/Misc/PackageName.h
#if WITH_EDITOR
#include "Misc/PackageName.h"

struct FNormalizedAssetPath {
  FString Path;
  bool bIsValid;
  FString ErrorMessage;
};

/**
 * Normalize an input asset path to a valid long package name and validate it.
 *
 * @param InPath The asset path or object path to normalize (may be short,
 * relative, or an object path).
 * @returns An FNormalizedAssetPath containing:
 *   - Path: the normalized package path candidate (may be unchanged if
 * invalid),
 *   - bIsValid: `true` when the path is a valid long package name and, when
 * applicable, the package exists,
 *   - ErrorMessage: populated with a validation error when `bIsValid` is
 * `false`.
 */
static inline FNormalizedAssetPath NormalizeAssetPath(const FString &InPath) {
  FNormalizedAssetPath Result;
  Result.bIsValid = false;

  if (InPath.IsEmpty()) {
    Result.ErrorMessage = TEXT("Asset path is empty");
    return Result;
  }

  FString CleanPath = InPath;

  // Remove trailing slashes
  while (CleanPath.EndsWith(TEXT("/"))) {
    CleanPath.RemoveAt(CleanPath.Len() - 1);
  }

  // Handle object paths (extract package name)
  // Object paths look like: /Game/Package.Object:SubObject
  FString PackageName = FPackageName::ObjectPathToPackageName(CleanPath);
  if (!PackageName.IsEmpty()) {
    CleanPath = PackageName;
  }

  // If path doesn't start with '/', try prepending /Game/
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/Game/") + CleanPath;
  }

  // Validate using engine API
  FText Reason;
  if (FPackageName::IsValidLongPackageName(CleanPath, true, &Reason)) {
    Result.Path = CleanPath;
    Result.bIsValid = true;
    return Result;
  }

  // If not in valid root, try other common roots
  TArray<FString> RootsToTry = {TEXT("/Game/"), TEXT("/Engine/"),
                                TEXT("/Script/")};
  FString BaseName = InPath;
  if (BaseName.StartsWith(TEXT("/"))) {
    // Extract just the asset name without the invalid root
    int32 LastSlash = -1;
    if (BaseName.FindLastChar(TEXT('/'), LastSlash) && LastSlash > 0) {
      BaseName = BaseName.RightChop(LastSlash + 1);
    }
  }

  for (const FString &Root : RootsToTry) {
    FString TestPath = Root + BaseName;
    FText DummyReason;
    if (FPackageName::IsValidLongPackageName(TestPath, true, &DummyReason)) {
      // Check if this asset actually exists
      if (FPackageName::DoesPackageExist(TestPath)) {
        Result.Path = TestPath;
        Result.bIsValid = true;
        return Result;
      }
    }
  }

  // Return what we have, with the validation error
  Result.Path = CleanPath;
  Result.ErrorMessage = FString::Printf(
      TEXT("Invalid asset path '%s': %s. Expected format: "
           "/Game/Folder/AssetName or /Engine/Folder/AssetName"),
      *InPath, *Reason.ToString());
  return Result;
}

// Convenience helper that tries to resolve the path and returns it, or empty if
// invalid Also outputs the resolved path to a pointer if provided
static inline FString TryResolveAssetPath(const FString &InPath,
                                          FString *OutResolvedPath = nullptr,
                                          FString *OutError = nullptr) {
  FNormalizedAssetPath Norm = NormalizeAssetPath(InPath);
  if (OutResolvedPath) {
    *OutResolvedPath = Norm.Path;
  }
  if (OutError && !Norm.bIsValid) {
    *OutError = Norm.ErrorMessage;
  }
  return Norm.bIsValid ? Norm.Path : FString();
}

/**
 * Resolves an asset path from a partial path or short name.
 * 1. Checks if InputPath exists exactly.
 * 2. If not, and InputPath is a short name, searches AssetRegistry.
 * 3. Returns the full package name if found uniquely.
 */
static inline FString ResolveAssetPath(const FString &InputPath) {
  if (InputPath.IsEmpty())
    return FString();

  // 1. Exact match check
  if (UEditorAssetLibrary::DoesAssetExist(InputPath)) {
    return InputPath;
  }

  // 2. Exact match with /Game/ prepended if it looks like a relative path but
  // missing root
  if (!InputPath.StartsWith(TEXT("/"))) {
    FString GamePath = TEXT("/Game/") + InputPath;
    if (UEditorAssetLibrary::DoesAssetExist(GamePath)) {
      return GamePath;
    }
  }

  // 3. Search by name if it's a short name (no slashes)
  // UE 5.7+ compatible: Use GetAssetsByPath + manual name filtering instead of FARFilter::AssetName
  // PERFORMANCE NOTE: This scans all assets under /Game when given a short name (no slashes).
  // For large projects, this could be slow if called frequently. Consider caching results
  // or providing full paths when possible.
  if (!InputPath.Contains(TEXT("/"))) {
    FString ShortName = FPaths::GetBaseFilename(InputPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> FoundAssets;
    TArray<FAssetData> AllGameAssets;

    // Use GetAssetsByPath with recursive search - more efficient than GetAllAssets
    AssetRegistry.GetAssetsByPath(FName(TEXT("/Game")), AllGameAssets, /*bRecursive=*/true);

    // Filter by name match (case-insensitive)
    for (const FAssetData &Asset : AllGameAssets) {
      if (Asset.AssetName.ToString().Equals(ShortName, ESearchCase::IgnoreCase)) {
        FoundAssets.Add(Asset);
      }
    }

    // Return unique match
    if (FoundAssets.Num() == 1) {
      return FoundAssets[0].PackageName.ToString();
    }

    // Multiple matches - prefer /Game/ assets
    if (FoundAssets.Num() > 1) {
      for (const FAssetData &Data : FoundAssets) {
        if (Data.PackageName.ToString().StartsWith(TEXT("/Game/"))) {
          return Data.PackageName.ToString();
        }
      }
      // Return first match if none start with /Game/
      return FoundAssets[0].PackageName.ToString();
    }
  }

  return FString();
}
#endif
