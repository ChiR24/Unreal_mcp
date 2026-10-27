#pragma once

#include "CoreMinimal.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#include "EditorAssetLibrary.h"

// Asset-registry directory entries can be stale, so creation validation uses
// the physical content directory whenever the mount can be resolved.
static inline bool DoesAssetDirectoryExistOnDisk(const FString &AssetPath) {
  FString NormalizedPath = AssetPath;
  NormalizedPath.RemoveFromEnd(TEXT("/"));
  if (NormalizedPath.Equals(TEXT("/Game"), ESearchCase::IgnoreCase) ||
      NormalizedPath.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase)) {
    return true;
  }
  // Maps every mount point (/Game, /Engine, plugins) to its content folder.
  FString FileSystemPath;
  if (!FPackageName::TryConvertLongPackageNameToFilename(NormalizedPath, FileSystemPath)) {
    return UEditorAssetLibrary::DoesDirectoryExist(AssetPath);
  }
  return IFileManager::Get().DirectoryExists(*FileSystemPath);
}
