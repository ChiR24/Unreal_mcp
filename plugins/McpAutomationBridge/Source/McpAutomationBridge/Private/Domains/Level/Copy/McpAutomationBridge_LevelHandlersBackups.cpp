#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace McpLevelHandlers {
namespace {
bool PathExists(const FString& Path, bool bDirectory) {
  return bDirectory ? IFileManager::Get().DirectoryExists(*Path)
                    : IFileManager::Get().FileExists(*Path);
}

bool CopyPath(const FString& To, const FString& From, bool bDirectory) {
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!bDirectory) {
    return PlatformFile.CopyFile(*To, *From);
  }
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
  return PlatformFile.CopyDirectoryTree(*To, *From, false);
}
} // namespace

bool DeleteLevelPath(const FString& Path, bool bDirectory) {
  return bDirectory ? IFileManager::Get().DeleteDirectory(*Path, false, true)
                    : IFileManager::Get().Delete(*Path, false, true, true);
}

bool BackupForOverwrite(const FString& Path,
                        bool bDirectory,
                        const TCHAR* Label,
                        bool& bExisted,
                        FString& BackupPath,
                        FString& ErrorMessage,
                        FString& ErrorCode) {
  bExisted = PathExists(Path, bDirectory);
  BackupPath.Reset();
  if (!bExisted) {
    return true;
  }
  // A directory backup keeps a package-safe name; a file backup gets a suffix no loader reads.
  const FString Candidate = Path + (bDirectory ? TEXT("_mcp_backup_") : TEXT(".mcp_backup_")) +
                            FGuid::NewGuid().ToString(EGuidFormats::Digits);
  if (!CopyPath(Candidate, Path, bDirectory)) {
    ErrorMessage = FString::Printf(TEXT("Failed to back up %s before overwrite: %s"),
                                   Label, *Path);
    ErrorCode = TEXT("DESTINATION_BACKUP_FAILED");
    return false;
  }
  if (!DeleteLevelPath(Path, bDirectory)) {
    DeleteLevelPath(Candidate, bDirectory);
    ErrorMessage = FString::Printf(TEXT("Failed to prepare %s for overwrite: %s"),
                                   Label, *Path);
    ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
    return false;
  }
  BackupPath = Candidate;
  return true;
}

bool RestoreBackup(const FString& Path, const FString& BackupPath, bool bDirectory) {
  if (BackupPath.IsEmpty() || !PathExists(BackupPath, bDirectory)) {
    return true;
  }
  DeleteLevelPath(Path, bDirectory);
  const bool bRestored = CopyPath(Path, BackupPath, bDirectory);
  if (bRestored) {
    DeleteLevelPath(BackupPath, bDirectory);
  }
  return bRestored;
}
} // namespace McpLevelHandlers
