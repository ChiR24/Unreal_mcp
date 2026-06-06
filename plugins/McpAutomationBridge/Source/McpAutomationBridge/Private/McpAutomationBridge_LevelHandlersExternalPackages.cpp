#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
bool GetExternalPackageDirectory(const FString& PackagePath,
                                 const FString& RootDirectoryName,
                                 FString& OutDirectory) {
  if (!IsGameLevelPackagePath(PackagePath)) {
    return false;
  }
  const FString RelativePackagePath = PackagePath.RightChop(6);
  if (RelativePackagePath.IsEmpty()) {
    return false;
  }
  OutDirectory = FPaths::ConvertRelativePathToFull(
      FPaths::ProjectContentDir() / RootDirectoryName / RelativePackagePath);
  FPaths::NormalizeDirectoryName(OutDirectory);
  return IsUnderProjectContentDir(OutDirectory);
}

bool BuildExternalPackageDirectoryCopyPlan(
    const FString& SourcePackagePath,
    const FString& DestinationPackagePath,
    const FString& RootDirectoryName,
    bool bOverwrite,
    FExternalPackageDirectoryCopyPlan& Plan,
    FString& ErrorMessage,
    FString& ErrorCode) {
  if (!GetExternalPackageDirectory(SourcePackagePath, RootDirectoryName,
                                   Plan.SourceDirectory) ||
      !GetExternalPackageDirectory(DestinationPackagePath, RootDirectoryName,
                                   Plan.DestinationDirectory)) {
    return true;
  }

  IFileManager& FileManager = IFileManager::Get();
  Plan.bSourceExists = FileManager.DirectoryExists(*Plan.SourceDirectory);
  Plan.bDestinationExists = FileManager.DirectoryExists(*Plan.DestinationDirectory);
  if (Plan.SourceDirectory.Equals(Plan.DestinationDirectory,
                                  ESearchCase::IgnoreCase)) {
    ErrorMessage = FString::Printf(
        TEXT("Source and destination external package directories are identical: %s"),
        *Plan.SourceDirectory);
    ErrorCode = TEXT("SAME_PATH");
    return false;
  }

  if (Plan.bDestinationExists && !bOverwrite) {
    ErrorMessage = FString::Printf(
        TEXT("Destination external package directory already exists: %s"),
        *Plan.DestinationDirectory);
    ErrorCode = TEXT("DESTINATION_EXISTS");
    return false;
  }
  return true;
}

bool CopyExternalPackageDirectory(FExternalPackageDirectoryCopyPlan& Plan,
                                  FString& ErrorMessage,
                                  FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  if (!Plan.bSourceExists) {
    if (Plan.bDestinationExists) {
      Plan.bDeletedDestination = FileManager.DeleteDirectory(
          *Plan.DestinationDirectory, false, true);
      if (!Plan.bDeletedDestination) {
        ErrorMessage = FString::Printf(
            TEXT("Failed to remove stale destination external package directory: %s"),
            *Plan.DestinationDirectory);
        ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
        return false;
      }
    }
    return true;
  }

  if (Plan.bDestinationExists) {
    Plan.bDeletedDestination = FileManager.DeleteDirectory(
        *Plan.DestinationDirectory, false, true);
    if (!Plan.bDeletedDestination) {
      ErrorMessage = FString::Printf(
          TEXT("Failed to overwrite destination external package directory: %s"),
          *Plan.DestinationDirectory);
      ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
      return false;
    }
  }

  FileManager.MakeDirectory(*FPaths::GetPath(Plan.DestinationDirectory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  Plan.bCopied = PlatformFile.CopyDirectoryTree(
      *Plan.DestinationDirectory, *Plan.SourceDirectory, false);
  if (!Plan.bCopied) {
    ErrorMessage = FString::Printf(
        TEXT("Failed to copy external package directory from %s to %s"),
        *Plan.SourceDirectory, *Plan.DestinationDirectory);
    ErrorCode = TEXT("COPY_FAILED");
    return false;
  }
  return true;
}

bool DeleteExternalPackageDirectory(const FString& PackagePath,
                                    const FString& RootDirectoryName,
                                    bool& bSourceExists,
                                    bool& bDeleted,
                                    FString& ErrorMessage,
                                    FString& ErrorCode) {
  FString Directory;
  if (!GetExternalPackageDirectory(PackagePath, RootDirectoryName, Directory)) {
    bSourceExists = false;
    bDeleted = false;
    return true;
  }

  IFileManager& FileManager = IFileManager::Get();
  bSourceExists = FileManager.DirectoryExists(*Directory);
  if (!bSourceExists) {
    bDeleted = false;
    return true;
  }

  bDeleted = FileManager.DeleteDirectory(*Directory, false, true);
  if (!bDeleted) {
    ErrorMessage = FString::Printf(
        TEXT("Failed to delete source external package directory: %s"),
        *Directory);
    ErrorCode = TEXT("SOURCE_EXTERNAL_DELETE_FAILED");
    return false;
  }
  return true;
}
#endif
} // namespace McpLevelHandlers
