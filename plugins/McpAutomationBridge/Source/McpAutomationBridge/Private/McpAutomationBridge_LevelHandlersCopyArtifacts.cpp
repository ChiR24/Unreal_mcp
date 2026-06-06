
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
namespace {
bool CopyBuiltDataIfPresent(FLevelCopyContext& Context,
                            TSharedPtr<FJsonObject>& Result,
                            FString& ErrorMessage,
                            FString& ErrorCode) {
  if (!Context.bSourceBuiltDataExists) {
    return true;
  }
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Context.DestinationBuiltDataFilename), true);
  Context.bCopiedBuiltData = PlatformFile.CopyFile(
      *Context.DestinationBuiltDataFilename, *Context.SourceBuiltDataFilename);
  if (Context.bCopiedBuiltData) {
    return true;
  }

  FString RollbackError;
  const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(Context, Result, RollbackError);
  ErrorMessage = FString::Printf(
      TEXT("Failed to copy built data from %s to %s"),
      *Context.SourceBuiltDataPackagePath, *Context.DestinationBuiltDataPackagePath);
  if (!bRollbackSucceeded) {
    ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
  }
  ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
  return false;
}

bool CopyExternalDirectoryIfPresent(FExternalPackageDirectoryCopyPlan& Plan,
                                    const TCHAR* Label,
                                    FLevelCopyContext& Context,
                                    TSharedPtr<FJsonObject>& Result,
                                    FString& ErrorMessage,
                                    FString& ErrorCode) {
  if (!Plan.bSourceExists) {
    return true;
  }
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Plan.DestinationDirectory), true);
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  Plan.bCopied = PlatformFile.CopyDirectoryTree(
      *Plan.DestinationDirectory, *Plan.SourceDirectory, false);
  if (Plan.bCopied) {
    return true;
  }

  FString RollbackError;
  const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(Context, Result, RollbackError);
  ErrorMessage = FString::Printf(TEXT("Failed to copy %s from %s to %s"), Label,
                                 *Plan.SourceDirectory, *Plan.DestinationDirectory);
  if (!bRollbackSucceeded) {
    ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
  }
  ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
  return false;
}
} // namespace

bool CopyLevelMapAndArtifacts(FLevelCopyContext& Context,
                              TSharedPtr<FJsonObject>& Result,
                              FString& ErrorMessage,
                              FString& ErrorCode) {
  IFileManager& FileManager = IFileManager::Get();
  const FString DestinationDir = FPaths::GetPath(Context.DestinationFilename);
  if (!DestinationDir.IsEmpty()) {
    FileManager.MakeDirectory(*DestinationDir, true);
  }

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  Context.bCopiedMap = PlatformFile.CopyFile(*Context.DestinationFilename,
                                             *Context.SourceFilename);
  if (!Context.bCopiedMap) {
    FString RollbackError;
    const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(Context, Result, RollbackError);
    ErrorMessage = FString::Printf(TEXT("Failed to copy level file from %s to %s"),
                                   *Context.SourcePackagePath,
                                   *Context.DestinationPackagePath);
    if (!bRollbackSucceeded) {
      ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
    }
    ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
    return false;
  }

  if (!CopyBuiltDataIfPresent(Context, Result, ErrorMessage, ErrorCode) ||
      !CopyExternalDirectoryIfPresent(Context.ExternalActorsPlan, TEXT("external actors"),
                                      Context, Result, ErrorMessage, ErrorCode) ||
      !CopyExternalDirectoryIfPresent(Context.ExternalObjectsPlan, TEXT("external objects"),
                                      Context, Result, ErrorMessage, ErrorCode)) {
    return false;
  }

  DeleteLevelCopyDestinationBackups(Context);
  return true;
}
#endif
} // namespace McpLevelHandlers
