#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#endif

static inline bool FindBlueprintNormalizedPath(const FString &Req,
                                               FString &OutNormalized) {
  OutNormalized.Empty();
  if (Req.IsEmpty())
    return false;
#if WITH_EDITOR
  FString CheckPath = Req;

  if (CheckPath.EndsWith(TEXT(".uasset"))) {
    CheckPath = CheckPath.LeftChop(7);
  }

  int32 DotIdx;
  if (CheckPath.FindLastChar(TEXT('.'), DotIdx)) {
    const FString AfterDot = CheckPath.Mid(DotIdx + 1);
    const FString BeforeDot = CheckPath.Left(DotIdx);
    int32 LastSlashIdx;
    if (BeforeDot.FindLastChar(TEXT('/'), LastSlashIdx)) {
      const FString AssetName = BeforeDot.Mid(LastSlashIdx + 1);
      if (AssetName.Equals(AfterDot, ESearchCase::IgnoreCase)) {
        CheckPath = BeforeDot;
      }
    }
  }

  if (!CheckPath.StartsWith(TEXT("/Game/")) &&
      !CheckPath.StartsWith(TEXT("/Engine/")) &&
      !CheckPath.StartsWith(TEXT("/Script/"))) {
    if (CheckPath.StartsWith(TEXT("/")) &&
        FPackageName::IsValidLongPackageName(CheckPath, true)) {
      // Preserve registered plugin mount points.
    } else if (CheckPath.StartsWith(TEXT("/"))) {
      CheckPath = TEXT("/Game") + CheckPath;
    } else {
      CheckPath = TEXT("/Game/") + CheckPath;
    }
  }

  if (UEditorAssetLibrary::DoesAssetExist(CheckPath)) {
    OutNormalized = CheckPath;
    return true;
  }
#endif
  return false;
}
