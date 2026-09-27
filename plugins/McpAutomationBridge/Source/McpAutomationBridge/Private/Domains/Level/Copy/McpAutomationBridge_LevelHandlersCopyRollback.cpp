#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpLevelHandlers {
namespace {
// One destination artifact a copy may overwrite: its path, its backup, and whether it existed.
struct FDestinationArtifact {
  const FString* Path;
  FString* Backup;
  bool* bExisted;
  bool bDirectory;
  const TCHAR* Label;
};

TArray<FDestinationArtifact, TInlineAllocator<4>> DestinationArtifacts(FLevelCopyContext& C) {
  return {
      {&C.DestinationFilename, &C.DestinationMapBackup, &C.bDeletedDestinationMap, false,
       TEXT("destination level")},
      {&C.DestinationBuiltDataFilename, &C.DestinationBuiltDataBackup,
       &C.bDeletedDestinationBuiltData, false, TEXT("destination built data")},
      {&C.ExternalActorsPlan.DestinationDirectory, &C.DestinationExternalActorsBackup,
       &C.ExternalActorsPlan.bDeletedDestination, true, TEXT("destination external actors")},
      {&C.ExternalObjectsPlan.DestinationDirectory, &C.DestinationExternalObjectsBackup,
       &C.ExternalObjectsPlan.bDeletedDestination, true, TEXT("destination external objects")}};
}

bool RestoreDestinationBackups(FLevelCopyContext& Context, FString& RollbackError) {
  bool bRollbackSucceeded = true;
  for (const FDestinationArtifact& Artifact : DestinationArtifacts(Context)) {
    if (!RestoreBackup(*Artifact.Path, *Artifact.Backup, Artifact.bDirectory)) {
      bRollbackSucceeded = false;
      RollbackError += FString::Printf(TEXT("%sfailed to restore %s backup: %s"),
                                       RollbackError.IsEmpty() ? TEXT("") : TEXT("; "),
                                       Artifact.Label, **Artifact.Backup);
    }
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
  for (const FDestinationArtifact& Artifact : DestinationArtifacts(Context)) {
    // A level with no BuiltData package has no BuiltData filename.
    if (Artifact.Path->IsEmpty() ||
        BackupForOverwrite(*Artifact.Path, Artifact.bDirectory, Artifact.Label,
                           *Artifact.bExisted, *Artifact.Backup, ErrorMessage, ErrorCode)) {
      continue;
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
  return true;
}

bool RollbackCopiedDestinationArtifacts(FLevelCopyContext& Context,
                                        TSharedPtr<FJsonObject>& Result,
                                        FString& RollbackError) {
  // What this copy created where nothing stood before goes; what it overwrote comes back from its backup.
  for (const FDestinationArtifact& Artifact : DestinationArtifacts(Context)) {
    if (Artifact.Backup->IsEmpty() && !Artifact.Path->IsEmpty()) {
      DeleteLevelPath(*Artifact.Path, Artifact.bDirectory);
    }
  }
  const bool bRollbackSucceeded = RestoreDestinationBackups(Context, RollbackError);
  RecordRollbackResult(Result, bRollbackSucceeded, RollbackError);
  return bRollbackSucceeded;
}

void DeleteLevelCopyDestinationBackups(FLevelCopyContext& Context) {
  for (const FDestinationArtifact& Artifact : DestinationArtifacts(Context)) {
    if (!Artifact.Backup->IsEmpty()) {
      DeleteLevelPath(*Artifact.Backup, Artifact.bDirectory);
    }
  }
}
} // namespace McpLevelHandlers
