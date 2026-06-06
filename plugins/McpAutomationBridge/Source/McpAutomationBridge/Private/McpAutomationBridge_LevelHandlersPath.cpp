#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
bool IsSafeLevelConsoleToken(const FString& Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return !Trimmed.IsEmpty() && !Trimmed.Contains(TEXT("\n")) &&
         !Trimmed.Contains(TEXT("\r")) && !Trimmed.Contains(TEXT("&&")) &&
         !Trimmed.Contains(TEXT("||")) && !Trimmed.Contains(TEXT(";")) &&
         !Trimmed.Contains(TEXT("|")) && !Trimmed.Contains(TEXT("`")) &&
         !Trimmed.Contains(TEXT(" ")) && !Trimmed.Contains(TEXT("\t"));
}

FString NormalizeLevelPackagePath(const FString& InPath) {
  FString PackagePath = InPath;
  int32 ObjectDelimiter = INDEX_NONE;
  if (PackagePath.FindChar(TEXT('.'), ObjectDelimiter)) {
    PackagePath = PackagePath.Left(ObjectDelimiter);
  }
  return PackagePath;
}

bool TryGetAbsoluteMapFilename(const FString& PackagePath, FString& OutFilename) {
  FString RelativeFilename;
  if (!FPackageName::TryConvertLongPackageNameToFilename(
          PackagePath, RelativeFilename, FPackageName::GetMapPackageExtension())) {
    return false;
  }
  OutFilename = FPaths::ConvertRelativePathToFull(RelativeFilename);
  FPaths::NormalizeFilename(OutFilename);
  return true;
}

bool IsGameLevelPackagePath(const FString& PackagePath) {
  return PackagePath.StartsWith(TEXT("/Game/")) &&
         !PackagePath.Contains(TEXT(".."));
}

bool IsUnderProjectContentDir(const FString& AbsolutePath) {
  FString ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
  FPaths::NormalizeDirectoryName(ProjectContentDir);
  if (!ProjectContentDir.EndsWith(TEXT("/"))) {
    ProjectContentDir += TEXT("/");
  }

  FString NormalizedPath = AbsolutePath;
  FPaths::NormalizeFilename(NormalizedPath);
  return NormalizedPath.StartsWith(ProjectContentDir, ESearchCase::IgnoreCase);
}

bool ValidateWritableGameMapPath(const FString& PackagePath,
                                 const FString& AbsoluteMapFilename,
                                 const TCHAR* Label,
                                 FString& ErrorMessage,
                                 FString& ErrorCode) {
  if (!IsGameLevelPackagePath(PackagePath)) {
    ErrorMessage = FString::Printf(
        TEXT("%s level path must be under /Game: %s"), Label, *PackagePath);
    ErrorCode = TEXT("SECURITY_VIOLATION");
    return false;
  }
  if (!IsUnderProjectContentDir(AbsoluteMapFilename)) {
    ErrorMessage = FString::Printf(
        TEXT("%s level file must be inside the project Content directory: %s"),
        Label, *PackagePath);
    ErrorCode = TEXT("SECURITY_VIOLATION");
    return false;
  }
  return true;
}

bool TryResolveWritableGameMapFilename(const FString& PackagePath,
                                       FString& OutFilename,
                                       FString& ErrorMessage,
                                       FString& ErrorCode,
                                       const TCHAR* Label) {
  if (!TryGetAbsoluteMapFilename(PackagePath, OutFilename)) {
    ErrorMessage = FString::Printf(
        TEXT("Could not convert %s level to filename: %s"), Label, *PackagePath);
    ErrorCode = TEXT("INVALID_LEVEL_PATH");
    return false;
  }
  return ValidateWritableGameMapPath(PackagePath, OutFilename, Label,
                                     ErrorMessage, ErrorCode);
}

void ScanLevelPackagePath(const FString& PackagePath, const FString& AbsoluteMapFilename) {
  IAssetRegistry& AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
  if (!AbsoluteMapFilename.IsEmpty() && IFileManager::Get().FileExists(*AbsoluteMapFilename)) {
    TArray<FString> FilesToScan;
    FilesToScan.Add(AbsoluteMapFilename);
    AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
  }
  const FString PackageDir = FPaths::GetPath(PackagePath);
  if (!PackageDir.IsEmpty()) {
    TArray<FString> PathsToScan;
    PathsToScan.Add(PackageDir);
    AssetRegistry.ScanPathsSynchronous(PathsToScan, false);
  }
}

bool IsCurrentEditorWorldPackage(const FString& PackagePath) {
  if (!GEditor) {
    return false;
  }
  UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
  return EditorWorld && EditorWorld->GetOutermost() &&
         EditorWorld->GetOutermost()->GetName() == PackagePath;
}
#endif
} // namespace McpLevelHandlers
