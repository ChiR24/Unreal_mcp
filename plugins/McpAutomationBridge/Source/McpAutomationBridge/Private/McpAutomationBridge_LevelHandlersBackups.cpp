#include "McpAutomationBridge_LevelHandlersCopyOperations.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
bool BackupFileForOverwrite(const FString& Filename,
                            const TCHAR* Label,
                            bool& bExisted,
                            FString& BackupFilename,
                            FString& ErrorMessage,
                            FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  bExisted = FileManager.FileExists(*Filename);
  BackupFilename.Reset();
  if (!bExisted) {
    return true;
  }

  BackupFilename = FString::Printf(TEXT("%s.mcp_backup_%s"), *Filename,
                                   *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.CopyFile(*BackupFilename, *Filename)) {
    ErrorMessage = FString::Printf(TEXT("Failed to back up %s before overwrite: %s"),
                                   Label, *Filename);
    ErrorCode = TEXT("DESTINATION_BACKUP_FAILED");
    BackupFilename.Reset();
    return false;
  }
  if (!FileManager.Delete(*Filename, false, true, true)) {
    FileManager.Delete(*BackupFilename, false, true, true);
    ErrorMessage = FString::Printf(TEXT("Failed to prepare %s for overwrite: %s"),
                                   Label, *Filename);
    ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
    BackupFilename.Reset();
    return false;
  }
  return true;
}

bool RestoreFileBackup(const FString& Filename, const FString& BackupFilename) {
  if (BackupFilename.IsEmpty() ||
      !IFileManager::Get().FileExists(*BackupFilename)) {
    return true;
  }
  IFileManager::Get().Delete(*Filename, false, true, true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  const bool bRestored = PlatformFile.CopyFile(*Filename, *BackupFilename);
  if (bRestored) {
    IFileManager::Get().Delete(*BackupFilename, false, true, true);
  }
  return bRestored;
}

void DeleteFileBackup(const FString& BackupFilename) {
  if (!BackupFilename.IsEmpty()) {
    IFileManager::Get().Delete(*BackupFilename, false, true, true);
  }
}

bool BackupDirectoryForOverwrite(const FString& Directory,
                                 const TCHAR* Label,
                                 bool& bExisted,
                                 FString& BackupDirectory,
                                 FString& ErrorMessage,
                                 FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  bExisted = FileManager.DirectoryExists(*Directory);
  BackupDirectory.Reset();
  if (!bExisted) {
    return true;
  }

  BackupDirectory = FString::Printf(TEXT("%s_mcp_backup_%s"), *Directory,
                                    *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  FileManager.MakeDirectory(*FPaths::GetPath(BackupDirectory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.CopyDirectoryTree(*BackupDirectory, *Directory, false)) {
    ErrorMessage = FString::Printf(TEXT("Failed to back up %s before overwrite: %s"),
                                   Label, *Directory);
    ErrorCode = TEXT("DESTINATION_BACKUP_FAILED");
    BackupDirectory.Reset();
    return false;
  }
  if (!FileManager.DeleteDirectory(*Directory, false, true)) {
    FileManager.DeleteDirectory(*BackupDirectory, false, true);
    ErrorMessage = FString::Printf(TEXT("Failed to prepare %s for overwrite: %s"),
                                   Label, *Directory);
    ErrorCode = TEXT("DESTINATION_DELETE_FAILED");
    BackupDirectory.Reset();
    return false;
  }
  return true;
}

bool RestoreDirectoryBackup(const FString& Directory, const FString& BackupDirectory) {
  if (BackupDirectory.IsEmpty() ||
      !IFileManager::Get().DirectoryExists(*BackupDirectory)) {
    return true;
  }
  IFileManager::Get().DeleteDirectory(*Directory, false, true);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Directory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  const bool bRestored = PlatformFile.CopyDirectoryTree(*Directory, *BackupDirectory, false);
  if (bRestored) {
    IFileManager::Get().DeleteDirectory(*BackupDirectory, false, true);
  }
  return bRestored;
}

void DeleteDirectoryBackup(const FString& BackupDirectory) {
  if (!BackupDirectory.IsEmpty()) {
    IFileManager::Get().DeleteDirectory(*BackupDirectory, false, true);
  }
}
#endif
} // namespace McpLevelHandlers
