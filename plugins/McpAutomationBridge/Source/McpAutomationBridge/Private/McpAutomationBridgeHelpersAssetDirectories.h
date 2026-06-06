#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif

// Asset-registry directory entries can be stale, so creation validation uses
// the physical content directory whenever the mount can be resolved.
static inline bool DoesAssetDirectoryExistOnDisk(const FString &AssetPath) {
  if (AssetPath.Equals(TEXT("/Game"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Game/"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase) ||
      AssetPath.Equals(TEXT("/Engine/"), ESearchCase::IgnoreCase)) {
    return true;
  }

  FString NormalizedPath = AssetPath;
  if (NormalizedPath.EndsWith(TEXT("/"))) {
    NormalizedPath.RemoveAt(NormalizedPath.Len() - 1);
  }

  FString FileSystemPath;
  if (NormalizedPath.StartsWith(TEXT("/Game/"))) {
    const FString RelativePath = NormalizedPath.RightChop(6);
    FileSystemPath = FPaths::ProjectContentDir() / RelativePath;
  } else if (NormalizedPath.StartsWith(TEXT("/Engine/"))) {
    const FString RelativePath = NormalizedPath.RightChop(8);
    FileSystemPath = FPaths::EngineContentDir() / RelativePath;
  } else {
    const FString PackageName = NormalizedPath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName,
                                                            FileSystemPath)) {
      return UEditorAssetLibrary::DoesDirectoryExist(AssetPath);
    }
  }

  return IFileManager::Get().DirectoryExists(*FileSystemPath);
}
#endif
