
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"

#include "McpHandlerUtils.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
void PopulateLevelCopyResult(FLevelCopyContext& Context,
                             TSharedPtr<FJsonObject>& Result) {
  Result = McpHandlerUtils::CreateResultObject();
  ScanLevelPackagePath(Context.DestinationPackagePath, Context.DestinationFilename);
  const bool bDestinationFileExists = IFileManager::Get().FileExists(*Context.DestinationFilename);
  const bool bDestinationPackageExists = FPackageName::DoesPackageExist(Context.DestinationPackagePath);

  Result->SetStringField(TEXT("sourcePath"), Context.SourcePackagePath);
  Result->SetStringField(TEXT("destinationPath"), Context.DestinationPackagePath);
  Result->SetStringField(TEXT("sourceFilename"), Context.SourceFilename);
  Result->SetStringField(TEXT("destinationFilename"), Context.DestinationFilename);
  Result->SetBoolField(TEXT("overwrite"), Context.bOverwrite);
  Result->SetBoolField(TEXT("copiedMapFile"), Context.bCopiedMap);
  Result->SetBoolField(TEXT("destinationMapExisted"), Context.bDestinationMapExists);
  Result->SetBoolField(TEXT("deletedDestinationMap"), Context.bDeletedDestinationMap);
  Result->SetBoolField(TEXT("sourceBuiltDataExists"), Context.bSourceBuiltDataExists);
  Result->SetBoolField(TEXT("destinationBuiltDataExisted"), Context.bDestinationBuiltDataExists);
  Result->SetBoolField(TEXT("deletedDestinationBuiltData"), Context.bDeletedDestinationBuiltData);
  Result->SetBoolField(TEXT("copiedBuiltData"), Context.bCopiedBuiltData);
  Result->SetBoolField(TEXT("sourceExternalActorsExists"), Context.ExternalActorsPlan.bSourceExists);
  Result->SetBoolField(TEXT("destinationExternalActorsExisted"), Context.ExternalActorsPlan.bDestinationExists);
  Result->SetBoolField(TEXT("deletedDestinationExternalActors"), Context.ExternalActorsPlan.bDeletedDestination);
  Result->SetBoolField(TEXT("copiedExternalActors"), Context.ExternalActorsPlan.bCopied);
  Result->SetBoolField(TEXT("sourceExternalObjectsExists"), Context.ExternalObjectsPlan.bSourceExists);
  Result->SetBoolField(TEXT("destinationExternalObjectsExisted"), Context.ExternalObjectsPlan.bDestinationExists);
  Result->SetBoolField(TEXT("deletedDestinationExternalObjects"), Context.ExternalObjectsPlan.bDeletedDestination);
  Result->SetBoolField(TEXT("copiedExternalObjects"), Context.ExternalObjectsPlan.bCopied);
  Result->SetBoolField(TEXT("destinationFileExists"), bDestinationFileExists);
  Result->SetBoolField(TEXT("destinationPackageExists"), bDestinationPackageExists);
}

bool CopyLevelMapPackageFile(const FString& SourcePackagePath,
                             const FString& DestinationPackagePath,
                             bool bOverwrite,
                             TSharedPtr<FJsonObject>& Result,
                             FString& ErrorMessage,
                             FString& ErrorCode) {
  FLevelCopyContext Context;
  if (!InitializeLevelCopyContext(SourcePackagePath, DestinationPackagePath,
                                  bOverwrite, Context, ErrorMessage, ErrorCode) ||
      !BackupLevelCopyDestinations(Context, Result, ErrorMessage, ErrorCode) ||
      !CopyLevelMapAndArtifacts(Context, Result, ErrorMessage, ErrorCode)) {
    return false;
  }

  PopulateLevelCopyResult(Context, Result);
  const bool bDestinationFileExists =
      Result->GetBoolField(TEXT("destinationFileExists"));
  if (!Context.bCopiedMap || !bDestinationFileExists) {
    ErrorMessage = FString::Printf(TEXT("Failed to copy level file from %s to %s"),
                                   *SourcePackagePath, *DestinationPackagePath);
    ErrorCode = TEXT("COPY_FAILED");
    return false;
  }
  return true;
}
#endif
} // namespace McpLevelHandlers
