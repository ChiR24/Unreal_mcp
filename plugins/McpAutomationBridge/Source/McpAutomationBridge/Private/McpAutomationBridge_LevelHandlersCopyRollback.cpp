
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"

#include "HAL/FileManager.h"

#include "McpHandlerUtils.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
namespace {
void AppendRollbackFailure(FString& RollbackError, const FString& Detail) {
  if (!RollbackError.IsEmpty()) {
    RollbackError += TEXT("; ");
  }
  RollbackError += Detail;
}

bool RestoreDestinationBackups(FLevelCopyContext& Context, FString& RollbackError) {
  bool bRollbackSucceeded = true;
  if (!RestoreFileBackup(Context.DestinationFilename, Context.DestinationMapBackup)) {
    bRollbackSucceeded = false;
    AppendRollbackFailure(RollbackError,
                          FString::Printf(TEXT("failed to restore destination level backup: %s"),
                                          *Context.DestinationMapBackup));
  }
  if (!RestoreFileBackup(Context.DestinationBuiltDataFilename,
                         Context.DestinationBuiltDataBackup)) {
    bRollbackSucceeded = false;
    AppendRollbackFailure(RollbackError,
                          FString::Printf(TEXT("failed to restore destination built data backup: %s"),
                                          *Context.DestinationBuiltDataBackup));
  }
  if (!RestoreDirectoryBackup(Context.ExternalActorsPlan.DestinationDirectory,
                              Context.DestinationExternalActorsBackup)) {
    bRollbackSucceeded = false;
    AppendRollbackFailure(RollbackError,
                          FString::Printf(TEXT("failed to restore destination external actors backup: %s"),
                                          *Context.DestinationExternalActorsBackup));
  }
  if (!RestoreDirectoryBackup(Context.ExternalObjectsPlan.DestinationDirectory,
                              Context.DestinationExternalObjectsBackup)) {
    bRollbackSucceeded = false;
    AppendRollbackFailure(RollbackError,
                          FString::Printf(TEXT("failed to restore destination external objects backup: %s"),
                                          *Context.DestinationExternalObjectsBackup));
  }
  return bRollbackSucceeded;
}

void RecordRollbackResult(TSharedPtr<FJsonObject>& Result,
                          bool bRollbackSucceeded,
                          const FString& RollbackError) {
  if (!Result.IsValid()) {
    Result = McpHandlerUtils::CreateResultObject();
  }
  Result->SetBoolField(TEXT("rollbackSucceeded"), bRollbackSucceeded);
  if (!RollbackError.IsEmpty()) {
    Result->SetStringField(TEXT("rollbackError"), RollbackError);
  }
}
} // namespace

bool BackupLevelCopyDestinations(FLevelCopyContext& Context,
                                 TSharedPtr<FJsonObject>& Result,
                                 FString& ErrorMessage,
                                 FString& ErrorCode) {
  if (BackupFileForOverwrite(Context.DestinationFilename, TEXT("destination level"),
                             Context.bDeletedDestinationMap,
                             Context.DestinationMapBackup, ErrorMessage,
                             ErrorCode) &&
      (Context.DestinationBuiltDataFilename.IsEmpty() || BackupFileForOverwrite(
          Context.DestinationBuiltDataFilename, TEXT("destination built data"),
          Context.bDeletedDestinationBuiltData,
          Context.DestinationBuiltDataBackup, ErrorMessage, ErrorCode)) &&
      BackupDirectoryForOverwrite(Context.ExternalActorsPlan.DestinationDirectory,
                                  TEXT("destination external actors"),
                                  Context.ExternalActorsPlan.bDeletedDestination,
                                  Context.DestinationExternalActorsBackup,
                                  ErrorMessage, ErrorCode) &&
      BackupDirectoryForOverwrite(Context.ExternalObjectsPlan.DestinationDirectory,
                                  TEXT("destination external objects"),
                                  Context.ExternalObjectsPlan.bDeletedDestination,
                                  Context.DestinationExternalObjectsBackup,
                                  ErrorMessage, ErrorCode)) {
    return true;
  }

  FString RollbackError;
  const bool bRollbackSucceeded = RestoreDestinationBackups(Context, RollbackError);
  RecordRollbackResult(Result, bRollbackSucceeded, RollbackError);
  if (!bRollbackSucceeded) {
    ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
    ErrorCode = TEXT("ROLLBACK_FAILED");
  }
  return false;
}

bool RollbackCopiedDestinationArtifacts(FLevelCopyContext& Context,
                                        TSharedPtr<FJsonObject>& Result,
                                        FString& RollbackError) {
  IFileManager& FileManager = IFileManager::Get();
  if (Context.DestinationMapBackup.IsEmpty() && !Context.DestinationFilename.IsEmpty()) {
    FileManager.Delete(*Context.DestinationFilename, false, true, true);
  }
  if (Context.DestinationBuiltDataBackup.IsEmpty() &&
      !Context.DestinationBuiltDataFilename.IsEmpty()) {
    FileManager.Delete(*Context.DestinationBuiltDataFilename, false, true, true);
  }
  if (Context.DestinationExternalActorsBackup.IsEmpty() &&
      !Context.ExternalActorsPlan.DestinationDirectory.IsEmpty()) {
    FileManager.DeleteDirectory(*Context.ExternalActorsPlan.DestinationDirectory, false, true);
  }
  if (Context.DestinationExternalObjectsBackup.IsEmpty() &&
      !Context.ExternalObjectsPlan.DestinationDirectory.IsEmpty()) {
    FileManager.DeleteDirectory(*Context.ExternalObjectsPlan.DestinationDirectory, false, true);
  }
  const bool bRollbackSucceeded = RestoreDestinationBackups(Context, RollbackError);
  RecordRollbackResult(Result, bRollbackSucceeded, RollbackError);
  return bRollbackSucceeded;
}

void DeleteLevelCopyDestinationBackups(FLevelCopyContext& Context) {
  DeleteFileBackup(Context.DestinationMapBackup);
  DeleteFileBackup(Context.DestinationBuiltDataBackup);
  DeleteDirectoryBackup(Context.DestinationExternalActorsBackup);
  DeleteDirectoryBackup(Context.DestinationExternalObjectsBackup);
}
#endif
} // namespace McpLevelHandlers
