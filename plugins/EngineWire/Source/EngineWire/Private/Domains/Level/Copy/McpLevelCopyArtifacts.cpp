#include "Domains/Level/Copy/McpLevelCopyOperations.h"

namespace McpLevelHandlers {
namespace {
// After a copy step failed with ErrorMessage: rolls the copy back and says if that failed too. Always false.
bool FailLevelCopy(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode) {
  FString RollbackError;
  const bool bRollbackSucceeded = RollbackCopiedDestinationArtifacts(Context, Result, RollbackError);
  if (!bRollbackSucceeded) {
    ErrorMessage += FString::Printf(TEXT(" Rollback failed: %s"), *RollbackError);
  }
  ErrorCode = bRollbackSucceeded ? TEXT("COPY_FAILED") : TEXT("ROLLBACK_FAILED");
  return false;
}

bool CopyBuiltDataIfPresent(FLevelCopyContext& Context,
                            TSharedPtr<FJsonObject>& Result,
                            FString& ErrorMessage,
                            FString& ErrorCode) {
  if (!Context.bSourceBuiltDataExists) {
    return true;
  }
  Context.bCopiedBuiltData = CopyPath(Context.DestinationBuiltDataFilename, Context.SourceBuiltDataFilename, false);
  if (Context.bCopiedBuiltData) {
    return true;
  }

  ErrorMessage = FString::Printf(
      TEXT("Failed to copy built data from %s to %s"),
      *Context.SourceBuiltDataPackagePath, *Context.DestinationBuiltDataPackagePath);
  return FailLevelCopy(Context, Result, ErrorMessage, ErrorCode);
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
  Plan.bCopied = CopyPath(Plan.DestinationDirectory, Plan.SourceDirectory, true);
  if (Plan.bCopied) {
    return true;
  }

  ErrorMessage = FString::Printf(TEXT("Failed to copy %s from %s to %s"), Label,
                                 *Plan.SourceDirectory, *Plan.DestinationDirectory);
  return FailLevelCopy(Context, Result, ErrorMessage, ErrorCode);
}
} // namespace

bool CopyLevelMapAndArtifacts(FLevelCopyContext& Context,
                              TSharedPtr<FJsonObject>& Result,
                              FString& ErrorMessage,
                              FString& ErrorCode) {
  Context.bCopiedMap = CopyPath(Context.DestinationFilename, Context.SourceFilename, false);
  if (!Context.bCopiedMap) {
    ErrorMessage = FString::Printf(TEXT("Failed to copy level file from %s to %s"),
                                   *Context.SourcePackagePath,
                                   *Context.DestinationPackagePath);
    return FailLevelCopy(Context, Result, ErrorMessage, ErrorCode);
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
} // namespace McpLevelHandlers
